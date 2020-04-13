#include "../harfbuzz/src/hb.h"
#include "../harfbuzz/src/hb-ot.h"

typedef struct user_data_t {
  char *str;
  unsigned size;
  unsigned consumed;
} user_data_t;

/* Our modified iota, why not using libc's? it is going to be used
   in harfbuzzjs where libc isn't available */
static void _hb_reverse (char *buf, unsigned int len)
{
  unsigned start = 0, end = len - 1;
  while (start < end)
  {
    char c = buf[end];
    buf[end] = buf[start];
    buf[start] = c;
    start++; end--;
  }
}
static unsigned _hb_itoa (int32_t num, char *buf)
{
  unsigned int i = 0;
  hb_bool_t is_negative = num < 0;
  if (is_negative) num = -num;
  do
  {
    buf[i++] = '0' + num % 10;
    num /= 10;
  } while (num);
  if (is_negative) buf[i++] = '-';
  _hb_reverse (buf, i);
  buf[i] = '\0';
  return i;
}

#define ITOA_BUF_SIZE 12 // 10 digits in int32, 1 for negative sign, 1 for \0

static void
move_to (hb_position_t to_x, hb_position_t to_y, user_data_t *user_data)
{
  /* 4 = command character space + comma + array starts with 0 index + nul character space */
  if (user_data->consumed + 2 * ITOA_BUF_SIZE + 4 > user_data->size) return;
  user_data->str[user_data->consumed++] = 'M';
  user_data->consumed += _hb_itoa (to_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (to_y, user_data->str + user_data->consumed);
}

static void
line_to (hb_position_t to_x, hb_position_t to_y, user_data_t *user_data)
{
  if (user_data->consumed + 2 * ITOA_BUF_SIZE + 4 > user_data->size) return;
  user_data->str[user_data->consumed++] = 'L';
  user_data->consumed += _hb_itoa (to_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (to_y, user_data->str + user_data->consumed);
}

static void
quadratic_to (hb_position_t control_x, hb_position_t control_y,
	  hb_position_t to_x, hb_position_t to_y,
	  user_data_t *user_data)
{

  if (user_data->consumed + 4 * ITOA_BUF_SIZE + 6 > user_data->size) return;
  user_data->str[user_data->consumed++] = 'Q';
  user_data->consumed += _hb_itoa (control_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (control_y, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ' ';
  user_data->consumed += _hb_itoa (to_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (to_y, user_data->str + user_data->consumed);
}

static void
cubic_to (hb_position_t control1_x, hb_position_t control1_y,
	  hb_position_t control2_x, hb_position_t control2_y,
	  hb_position_t to_x, hb_position_t to_y,
	  user_data_t *user_data)
{
  if (user_data->consumed + 6 * ITOA_BUF_SIZE + 8 > user_data->size) return;
  user_data->str[user_data->consumed++] = 'C';
  user_data->consumed += _hb_itoa (control1_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (control1_y, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ' ';
  user_data->consumed += _hb_itoa (control2_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (control2_y, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ' ';
  user_data->consumed += _hb_itoa (to_x, user_data->str + user_data->consumed);
  user_data->str[user_data->consumed++] = ',';
  user_data->consumed += _hb_itoa (to_y, user_data->str + user_data->consumed);
}

static void
close_path (user_data_t *user_data)
{
  if (user_data->consumed + 2 > user_data->size) return;
  user_data->str[user_data->consumed++] = 'Z';
}

static hb_draw_funcs_t *funcs = 0;

unsigned
hbjs_glyph_svg (hb_font_t *font, hb_codepoint_t glyph, char *buf, unsigned buf_size)
{
  if (funcs == 0) /* not the best pattern for multi-threaded apps which is not a concern here */
  {
    funcs = hb_draw_funcs_create (); /* will be leaked */
    hb_draw_funcs_set_move_to_func (funcs, (hb_draw_move_to_func_t) move_to);
    hb_draw_funcs_set_line_to_func (funcs, (hb_draw_line_to_func_t) line_to);
    hb_draw_funcs_set_quadratic_to_func (funcs, (hb_draw_quadratic_to_func_t) quadratic_to);
    hb_draw_funcs_set_cubic_to_func (funcs, (hb_draw_cubic_to_func_t) cubic_to);
    hb_draw_funcs_set_close_path_func (funcs, (hb_draw_close_path_func_t) close_path);
  }

  user_data_t user_data = {
    .str = buf,
    .size = buf_size,
    .consumed = 0
  };
  hb_font_draw_glyph (font, glyph, funcs, &user_data);
  buf[user_data.consumed] = '\0';
  return user_data.consumed;
}

#ifdef MAIN
#include <stdio.h>
int main() {
    hb_blob_t *blob = hb_blob_create_from_file ("/home/ebrahim/Desktop/harfbuzzjs/harfbuzz/test/subset/data/fonts/Roboto-Regular.ttf");
    hb_face_t *face = hb_face_create (blob, 0);
    hb_blob_destroy (blob);
    hb_font_t *font = hb_font_create (face);
    hb_face_destroy (face);
    char buf[1024];
    buf[0] = '\0';
    printf ("%d %d\n", hb_blob_get_length (blob), hbjs_ot_glyph_svg (font, 0, buf, sizeof (buf)));
    puts (buf);
    hb_font_destroy (font);
}
#endif