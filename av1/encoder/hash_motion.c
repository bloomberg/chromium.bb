#include <assert.h>
#include "av1/encoder/hash_motion.h"
#include "./av1_rtcd.h"

typedef struct _crc_calculator {
  uint32_t remainder;
  uint32_t trunc_poly;
  uint32_t bits;
  uint32_t table[256];
  uint32_t final_result_mask;
} crc_calculator;

static void crc_calculator_process_data(crc_calculator *p_crc_calculator,
                                        uint8_t *pData, uint32_t dataLength) {
  for (uint32_t i = 0; i < dataLength; i++) {
    const uint8_t index =
        (p_crc_calculator->remainder >> (p_crc_calculator->bits - 8)) ^
        pData[i];
    p_crc_calculator->remainder <<= 8;
    p_crc_calculator->remainder ^= p_crc_calculator->table[index];
  }
}

static void crc_calculator_reset(crc_calculator *p_crc_calculator) {
  p_crc_calculator->remainder = 0;
}

static uint32_t crc_calculator_get_crc(crc_calculator *p_crc_calculator) {
  return p_crc_calculator->remainder & p_crc_calculator->final_result_mask;
}

static void crc_calculator_init_table(crc_calculator *p_crc_calculator) {
  const uint32_t high_bit = 1 << (p_crc_calculator->bits - 1);
  const uint32_t byte_high_bit = 1 << (8 - 1);

  for (uint32_t value = 0; value < 256; value++) {
    uint32_t remainder = 0;
    for (uint8_t mask = byte_high_bit; mask != 0; mask >>= 1) {
      if (value & mask) {
        remainder ^= high_bit;
      }

      if (remainder & high_bit) {
        remainder <<= 1;
        remainder ^= p_crc_calculator->trunc_poly;
      } else {
        remainder <<= 1;
      }
    }
    p_crc_calculator->table[value] = remainder;
  }
}

static void crc_calculator_init(crc_calculator *p_crc_calculator, uint32_t bits,
                                uint32_t truncPoly) {
  p_crc_calculator->remainder = 0;
  p_crc_calculator->bits = bits;
  p_crc_calculator->trunc_poly = truncPoly;
  p_crc_calculator->final_result_mask = (1 << bits) - 1;
  crc_calculator_init_table(p_crc_calculator);
}

static const int crc_bits = 16;
static const int block_size_bits = 2;

static crc_calculator crc_calculator1;
static crc_calculator crc_calculator2;
static int g_crc_initialized = 0;

static void hash_table_clear_all(hash_table *p_hash_table) {
  if (p_hash_table->p_lookup_table == NULL) {
    return;
  }
  int max_addr = 1 << (crc_bits + block_size_bits);
  for (int i = 0; i < max_addr; i++) {
    if (p_hash_table->p_lookup_table[i] != NULL) {
      vector_destroy(p_hash_table->p_lookup_table[i]);
      aom_free(p_hash_table->p_lookup_table[i]);
      p_hash_table->p_lookup_table[i] = NULL;
    }
  }
}

static uint32_t get_crc_value1(uint8_t *p, int length) {
  crc_calculator_reset(&crc_calculator1);
  crc_calculator_process_data(&crc_calculator1, p, length);
  return crc_calculator_get_crc(&crc_calculator1);
}

static uint32_t get_crc_value2(uint8_t *p, int length) {
  crc_calculator_reset(&crc_calculator2);
  crc_calculator_process_data(&crc_calculator2, p, length);
  return crc_calculator_get_crc(&crc_calculator2);
}

// TODO(youzhou@microsoft.com): is higher than 8 bits screen content supported?
// If yes, fix this function
static void get_pixels_in_1D_char_array_by_block_2x2(uint8_t *y_src, int stride,
                                                     uint8_t *p_pixels_in1D) {
  uint8_t *p_pel = y_src;
  int index = 0;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      p_pixels_in1D[index++] = p_pel[j];
    }
    p_pel += stride;
  }
}

static int is_block_2x2_row_same_value(uint8_t *p) {
  if (p[0] != p[1] || p[2] != p[3]) {
    return 0;
  }

  return 1;
}

static int is_block_2x2_col_same_value(uint8_t *p) {
  if ((p[0] != p[2]) || (p[1] != p[3])) {
    return 0;
  }

  return 1;
}

// the hash value (hash_value1 consists two parts, the first 2 bits relate to
// the block size and the remaining 16 bits are the crc values. This fuction
// is used to get the first 2 bits.
static int hash_block_size_to_index(int block_size) {
  switch (block_size) {
    case 8: return 0;
    case 16: return 1;
    case 32: return 2;
    case 64: return 3;
    default: return -1;
  }
}

void av1_hash_table_init(hash_table *p_hash_table) {
  if (g_crc_initialized == 0) {
    crc_calculator_init(&crc_calculator1, 24, 0x5D6DCB);
    crc_calculator_init(&crc_calculator2, 24, 0x864CFB);
    g_crc_initialized = 1;
  }
  p_hash_table->p_lookup_table = NULL;
}

void av1_hash_table_destroy(hash_table *p_hash_table) {
  hash_table_clear_all(p_hash_table);
  aom_free(p_hash_table->p_lookup_table);
  p_hash_table->p_lookup_table = NULL;
}

void av1_hash_table_create(hash_table *p_hash_table) {
  if (p_hash_table->p_lookup_table != NULL) {
    hash_table_clear_all(p_hash_table);
    return;
  }
  const int max_addr = 1 << (crc_bits + block_size_bits);
  p_hash_table->p_lookup_table =
      (Vector **)aom_malloc(sizeof(p_hash_table->p_lookup_table[0]) * max_addr);
  memset(p_hash_table->p_lookup_table, 0,
         sizeof(p_hash_table->p_lookup_table[0]) * max_addr);
}

static void hash_table_add_to_table(hash_table *p_hash_table,
                                    uint32_t hash_value,
                                    block_hash *curr_block_hash) {
  if (p_hash_table->p_lookup_table[hash_value] == NULL) {
    p_hash_table->p_lookup_table[hash_value] =
        aom_malloc(sizeof(p_hash_table->p_lookup_table[0][0]));
    vector_setup(p_hash_table->p_lookup_table[hash_value], 10,
                 sizeof(curr_block_hash[0]));
    vector_push_back(p_hash_table->p_lookup_table[hash_value], curr_block_hash);
  } else {
    vector_push_back(p_hash_table->p_lookup_table[hash_value], curr_block_hash);
  }
}

int32_t av1_hash_table_count(hash_table *p_hash_table, uint32_t hash_value) {
  if (p_hash_table->p_lookup_table[hash_value] == NULL) {
    return 0;
  } else {
    return (int32_t)(p_hash_table->p_lookup_table[hash_value]->size);
  }
}

Iterator av1_hash_get_first_iterator(hash_table *p_hash_table,
                                     uint32_t hash_value) {
  assert(av1_hash_table_count(p_hash_table, hash_value) > 0);
  return vector_begin(p_hash_table->p_lookup_table[hash_value]);
}

int32_t av1_has_exact_match(hash_table *p_hash_table, uint32_t hash_value1,
                            uint32_t hash_value2) {
  if (p_hash_table->p_lookup_table[hash_value1] == NULL) {
    return 0;
  }
  Iterator iterator = vector_begin(p_hash_table->p_lookup_table[hash_value1]);
  Iterator last = vector_end(p_hash_table->p_lookup_table[hash_value1]);
  for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) {
    if ((*(block_hash *)iterator_get(&iterator)).hash_value2 == hash_value2) {
      return 1;
    }
  }
  return 0;
}

void av1_generate_block_2x2_hash_value(const YV12_BUFFER_CONFIG *picture,
                                       uint32_t *pic_block_hash[2],
                                       int8_t *pic_block_same_info[3]) {
  const int width = 2;
  const int height = 2;
  const int x_end = picture->y_crop_width - width + 1;
  const int y_end = picture->y_crop_height - height + 1;

  const int length = width * 2;
  uint8_t p[4];

  int pos = 0;
  for (int y_pos = 0; y_pos < y_end; y_pos++) {
    for (int x_pos = 0; x_pos < x_end; x_pos++) {
      get_pixels_in_1D_char_array_by_block_2x2(
          picture->y_buffer + y_pos * picture->y_stride + x_pos,
          picture->y_stride, p);
      pic_block_same_info[0][pos] = is_block_2x2_row_same_value(p);
      pic_block_same_info[1][pos] = is_block_2x2_col_same_value(p);

      pic_block_hash[0][pos] = get_crc_value1(p, length * sizeof(p[0]));
      pic_block_hash[1][pos] = get_crc_value2(p, length * sizeof(p[0]));

      pos++;
    }
    pos += width - 1;
  }
}

void av1_generate_block_hash_value(const YV12_BUFFER_CONFIG *picture,
                                   int block_size,
                                   uint32_t *src_pic_block_hash[2],
                                   uint32_t *dst_pic_block_hash[2],
                                   int8_t *src_pic_block_same_info[3],
                                   int8_t *dst_pic_block_same_info[3]) {
  const int pic_width = picture->y_crop_width;
  const int x_end = picture->y_crop_width - block_size + 1;
  const int y_end = picture->y_crop_height - block_size + 1;

  const int src_size = block_size >> 1;
  const int quad_size = block_size >> 2;

  uint32_t p[4];
  const int length = sizeof(p);

  int pos = 0;
  for (int y_pos = 0; y_pos < y_end; y_pos++) {
    for (int x_pos = 0; x_pos < x_end; x_pos++) {
      p[0] = src_pic_block_hash[0][pos];
      p[1] = src_pic_block_hash[0][pos + src_size];
      p[2] = src_pic_block_hash[0][pos + src_size * pic_width];
      p[3] = src_pic_block_hash[0][pos + src_size * pic_width + src_size];
      dst_pic_block_hash[0][pos] = get_crc_value1((uint8_t *)p, length);

      p[0] = src_pic_block_hash[1][pos];
      p[1] = src_pic_block_hash[1][pos + src_size];
      p[2] = src_pic_block_hash[1][pos + src_size * pic_width];
      p[3] = src_pic_block_hash[1][pos + src_size * pic_width + src_size];
      dst_pic_block_hash[1][pos] = get_crc_value2((uint8_t *)p, length);

      dst_pic_block_same_info[0][pos] =
          src_pic_block_same_info[0][pos] &&
          src_pic_block_same_info[0][pos + quad_size] &&
          src_pic_block_same_info[0][pos + src_size] &&
          src_pic_block_same_info[0][pos + src_size * pic_width] &&
          src_pic_block_same_info[0][pos + src_size * pic_width + quad_size] &&
          src_pic_block_same_info[0][pos + src_size * pic_width + src_size];

      dst_pic_block_same_info[1][pos] =
          src_pic_block_same_info[1][pos] &&
          src_pic_block_same_info[1][pos + src_size] &&
          src_pic_block_same_info[1][pos + quad_size * pic_width] &&
          src_pic_block_same_info[1][pos + quad_size * pic_width + src_size] &&
          src_pic_block_same_info[1][pos + src_size * pic_width] &&
          src_pic_block_same_info[1][pos + src_size * pic_width + src_size];
      pos++;
    }
    pos += block_size - 1;
  }

  if (block_size >= 8) {
    const int size_minus1 = block_size - 1;
    pos = 0;
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      for (int x_pos = 0; x_pos < x_end; x_pos++) {
        dst_pic_block_same_info[2][pos] =
            (!dst_pic_block_same_info[0][pos] &&
             !dst_pic_block_same_info[1][pos]) ||
            (((x_pos & size_minus1) == 0) && ((y_pos & size_minus1) == 0));
        pos++;
      }
      pos += block_size - 1;
    }
  }
}

void av1_add_to_hash_map_by_row_with_precal_data(hash_table *p_hash_table,
                                                 uint32_t *pic_hash[2],
                                                 int8_t *pic_is_same,
                                                 int pic_width, int pic_height,
                                                 int block_size) {
  const int x_end = pic_width - block_size + 1;
  const int y_end = pic_height - block_size + 1;

  const int8_t *src_is_added = pic_is_same;
  const uint32_t *src_hash[2] = { pic_hash[0], pic_hash[1] };

  int add_value = hash_block_size_to_index(block_size);
  assert(add_value >= 0);
  add_value <<= crc_bits;
  const int crc_mask = (1 << crc_bits) - 1;

  for (int x_pos = 0; x_pos < x_end; x_pos++) {
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      const int pos = y_pos * pic_width + x_pos;
      // valid data
      if (src_is_added[pos]) {
        block_hash curr_block_hash;
        curr_block_hash.x = x_pos;
        curr_block_hash.y = y_pos;

        const uint32_t hash_value1 = (src_hash[0][pos] & crc_mask) + add_value;
        curr_block_hash.hash_value2 = src_hash[1][pos];

        hash_table_add_to_table(p_hash_table, hash_value1, &curr_block_hash);
      }
    }
  }
}

int av1_hash_is_horizontal_perfect(const YV12_BUFFER_CONFIG *picture,
                                   int block_size, int x_start, int y_start) {
  const int stride = picture->y_stride;
  const uint8_t *p = picture->y_buffer + y_start * stride + x_start;

  for (int i = 0; i < block_size; i++) {
    for (int j = 1; j < block_size; j++) {
      if (p[j] != p[0]) {
        return 0;
      }
    }
    p += stride;
  }

  return 1;
}

int av1_hash_is_vertical_perfect(const YV12_BUFFER_CONFIG *picture,
                                 int block_size, int x_start, int y_start) {
  const int stride = picture->y_stride;
  const uint8_t *p = picture->y_buffer + y_start * stride + x_start;

  for (int i = 0; i < block_size; i++) {
    for (int j = 1; j < block_size; j++) {
      if (p[j * stride + i] != p[i]) {
        return 0;
      }
    }
  }

  return 1;
}

// global buffer for hash value calculation of a block
// used only in av1_get_block_hash_value()
static uint32_t hash_value_buffer[2][2][1024];  // [first hash/second hash]
                                                // [two buffers used ping-pong]
                                                // [num of 2x2 blocks in 64x64]

void av1_get_block_hash_value(uint8_t *y_src, int stride, int block_size,
                              uint32_t *hash_value1, uint32_t *hash_value2) {
  uint8_t pixel_to_hash[4];
  uint32_t to_hash[4];
  const int add_value = hash_block_size_to_index(block_size) << crc_bits;
  assert(add_value >= 0);
  const int crc_mask = (1 << crc_bits) - 1;

  // 2x2 subblock hash values in current CU
  int sub_block_in_width = (block_size >> 1);
  for (int y_pos = 0; y_pos < block_size; y_pos += 2) {
    for (int x_pos = 0; x_pos < block_size; x_pos += 2) {
      int pos = (y_pos >> 1) * sub_block_in_width + (x_pos >> 1);
      get_pixels_in_1D_char_array_by_block_2x2(y_src + y_pos * stride + x_pos,
                                               stride, pixel_to_hash);

      hash_value_buffer[0][0][pos] =
          get_crc_value1(pixel_to_hash, sizeof(pixel_to_hash));
      hash_value_buffer[1][0][pos] =
          get_crc_value2(pixel_to_hash, sizeof(pixel_to_hash));
    }
  }

  int src_sub_block_in_width = sub_block_in_width;
  sub_block_in_width >>= 1;

  int src_idx = 1;
  int dst_idx = 0;

  // 4x4 subblock hash values to current block hash values
  for (int sub_width = 4; sub_width <= block_size; sub_width *= 2) {
    src_idx = 1 - src_idx;
    dst_idx = 1 - dst_idx;

    int dst_pos = 0;
    for (int y_pos = 0; y_pos < sub_block_in_width; y_pos++) {
      for (int x_pos = 0; x_pos < sub_block_in_width; x_pos++) {
        int srcPos = (y_pos << 1) * src_sub_block_in_width + (x_pos << 1);

        to_hash[0] = hash_value_buffer[0][src_idx][srcPos];
        to_hash[1] = hash_value_buffer[0][src_idx][srcPos + 1];
        to_hash[2] =
            hash_value_buffer[0][src_idx][srcPos + src_sub_block_in_width];
        to_hash[3] =
            hash_value_buffer[0][src_idx][srcPos + src_sub_block_in_width + 1];

        hash_value_buffer[0][dst_idx][dst_pos] =
            get_crc_value1((uint8_t *)to_hash, sizeof(to_hash));

        to_hash[0] = hash_value_buffer[1][src_idx][srcPos];
        to_hash[1] = hash_value_buffer[1][src_idx][srcPos + 1];
        to_hash[2] =
            hash_value_buffer[1][src_idx][srcPos + src_sub_block_in_width];
        to_hash[3] =
            hash_value_buffer[1][src_idx][srcPos + src_sub_block_in_width + 1];
        hash_value_buffer[1][dst_idx][dst_pos] =
            get_crc_value2((uint8_t *)to_hash, sizeof(to_hash));
        dst_pos++;
      }
    }

    src_sub_block_in_width = sub_block_in_width;
    sub_block_in_width >>= 1;
  }

  *hash_value1 = (hash_value_buffer[0][dst_idx][0] & crc_mask) + add_value;
  *hash_value2 = hash_value_buffer[1][dst_idx][0];
}
