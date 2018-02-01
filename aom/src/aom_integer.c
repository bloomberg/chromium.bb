/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "aom/aom_integer.h"

static const size_t kMaximumLeb128Size = 4;
static const size_t kMaximumLeb128Value = 0xFFFFFFF;  // 2^28 - 1
static const uint8_t kLeb128ByteMask = 0x7f;          // Binary: 01111111
static const uint8_t kLeb128PadByte = 0x80;           // Binary: 10000000

size_t aom_uleb_size_in_bytes(uint32_t value) {
  size_t size = 0;
  do {
    ++size;
  } while ((value >>= 7) != 0);
  return size;
}

void aom_uleb_decode(const uint8_t *buffer, size_t available, uint32_t *value) {
  if (buffer && value) {
    for (size_t i = 0; i < kMaximumLeb128Size && i < available; ++i) {
      const uint8_t decoded_byte = *(buffer + i) & kLeb128ByteMask;
      *value |= decoded_byte << (i * 7);
      if ((*(buffer + i) >> 7) == 0) break;
    }
  }
}

int aom_uleb_encode(uint32_t value, size_t available, uint8_t *coded_value,
                    size_t *coded_size) {
  const size_t leb_size = aom_uleb_size_in_bytes(value);
  if (value > kMaximumLeb128Value || leb_size > kMaximumLeb128Size ||
      leb_size > available || !coded_value || !coded_size) {
    return -1;
  }

  for (size_t i = 0; i < leb_size; ++i) {
    uint8_t byte = value & 0x7f;
    value >>= 7;

    if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

    *(coded_value + i) = byte;
  }

  *coded_size = leb_size;
  return 0;
}

int aom_uleb_encode_fixed_size(uint32_t value, size_t available,
                               size_t pad_to_size, uint8_t *coded_value,
                               size_t *coded_size) {
  if (!coded_value || !coded_size || available < pad_to_size ||
      pad_to_size > kMaximumLeb128Size ||
      aom_uleb_encode(value, available, coded_value, coded_size) != 0) {
    return -1;
  }

  if (pad_to_size != *coded_size) {
    if (*coded_size < pad_to_size) {
      const size_t num_pad_bytes = pad_to_size - *coded_size;
      for (size_t i = 0; i < num_pad_bytes; ++i) {
        const size_t offset = *coded_size + i;
        *(coded_value + offset) = kLeb128PadByte;
      }
      *coded_size += num_pad_bytes;
    }
  }

  return 0;
}
