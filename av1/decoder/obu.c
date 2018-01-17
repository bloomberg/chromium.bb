/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "./aom_config.h"

#include "aom/aom_codec.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_ports/mem_ops.h"

#include "av1/common/common.h"
#include "av1/decoder/decoder.h"
#include "av1/decoder/decodeframe.h"

static OBU_TYPE read_obu_header(struct aom_read_bit_buffer *rb,
                                size_t *header_size) {
  *header_size = 1;

  // first bit is obu_forbidden_bit (0) according to R19
  aom_rb_read_bit(rb);

  const OBU_TYPE obu_type = (OBU_TYPE)aom_rb_read_literal(rb, 4);
  aom_rb_read_literal(rb, 2);  // reserved
  const int obu_extension_flag = aom_rb_read_bit(rb);
  if (obu_extension_flag) {
    *header_size += 1;
    aom_rb_read_literal(rb, 3);  // temporal_id
    aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 1);  // reserved
  }

  return obu_type;
}

static uint32_t read_temporal_delimiter_obu() { return 0; }

static uint32_t read_sequence_header_obu(AV1Decoder *pbi,
                                         struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t saved_bit_offset = rb->bit_offset;

  cm->profile = av1_read_profile(rb);
  aom_rb_read_literal(rb, 4);  // level

  read_sequence_header(&cm->seq_params, rb);

  av1_read_bitdepth_colorspace_sampling(cm, rb, pbi->allow_lowbitdepth);

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_frame_header_obu(AV1Decoder *pbi, const uint8_t *data,
                                      const uint8_t *data_end,
                                      const uint8_t **p_data_end) {
  av1_decode_frame_headers_and_setup(pbi, data, data_end, p_data_end);
  return (uint32_t)(pbi->uncomp_hdr_size);
}

static uint32_t read_tile_group_header(AV1Decoder *pbi,
                                       struct aom_read_bit_buffer *rb,
                                       int *startTile, int *endTile) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t saved_bit_offset = rb->bit_offset;

#if CONFIG_EXT_TILE
  if (pbi->common.large_scale_tile) {
    *startTile = 0;
    *endTile = pbi->common.tile_rows * pbi->common.tile_cols - 1;
    return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
  }
#endif

  *startTile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);
  *endTile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_one_tile_group_obu(AV1Decoder *pbi,
                                        struct aom_read_bit_buffer *rb,
                                        int is_first_tg, const uint8_t *data,
                                        const uint8_t *data_end,
                                        const uint8_t **p_data_end,
                                        int *is_last_tg) {
  AV1_COMMON *const cm = &pbi->common;
  int startTile, endTile;
  uint32_t header_size, tg_payload_size;

  header_size = read_tile_group_header(pbi, rb, &startTile, &endTile);
  data += header_size;
  av1_decode_tg_tiles_and_wrapup(pbi, data, data_end, p_data_end, startTile,
                                 endTile, is_first_tg);
  tg_payload_size = (uint32_t)(*p_data_end - data);

  // TODO(shan):  For now, assume all tile groups received in order
  *is_last_tg = endTile == cm->tile_rows * cm->tile_cols - 1;

  return header_size + tg_payload_size;
}

static void read_metadata_private_data(const uint8_t *data, size_t sz) {
  for (size_t i = 0; i < sz; i++) {
    mem_get_le16(data);
    data += 2;
  }
}

static void read_metadata_hdr_cll(const uint8_t *data) {
  mem_get_le16(data);
  mem_get_le16(data + 2);
}

static void read_metadata_hdr_mdcv(const uint8_t *data) {
  for (int i = 0; i < 3; i++) {
    mem_get_le16(data);
    data += 2;
    mem_get_le16(data);
    data += 2;
  }

  mem_get_le16(data);
  data += 2;
  mem_get_le16(data);
  data += 2;
  mem_get_le16(data);
  data += 2;
  mem_get_le16(data);
}

static size_t read_metadata(const uint8_t *data, size_t sz) {
  assert(sz >= 2);
  const OBU_METADATA_TYPE metadata_type = (OBU_METADATA_TYPE)mem_get_le16(data);

  if (metadata_type == OBU_METADATA_TYPE_PRIVATE_DATA) {
    read_metadata_private_data(data + 2, sz - 2);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_CLL) {
    read_metadata_hdr_cll(data + 2);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_MDCV) {
    read_metadata_hdr_mdcv(data + 2);
  }

  return sz;
}

void av1_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
                                const uint8_t *data_end,
                                const uint8_t **p_data_end) {
  AV1_COMMON *const cm = &pbi->common;
  int frame_decoding_finished = 0;
  int is_first_tg_obu_received = 1;
  int frame_header_received = 0;
  int frame_header_size = 0;

  if (data_end < data) {
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return;
  }

  // decode frame as a series of OBUs
  while (!frame_decoding_finished && !cm->error.error_code) {
    struct aom_read_bit_buffer rb;
    size_t obu_header_size, obu_payload_size = 0;
    av1_init_read_bit_buffer(pbi, &rb, data + PRE_OBU_SIZE_BYTES, data_end);
    // every obu is preceded by PRE_OBU_SIZE_BYTES-byte size of obu (obu header
    // + payload size)
    // The obu size is only needed for tile group OBUs
    const size_t obu_size = mem_get_le32(data);
    const OBU_TYPE obu_type = read_obu_header(&rb, &obu_header_size);
    data += (PRE_OBU_SIZE_BYTES + obu_header_size);
    if (data_end < data) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
    switch (obu_type) {
      case OBU_TEMPORAL_DELIMITER:
        obu_payload_size = read_temporal_delimiter_obu();
        break;
      case OBU_SEQUENCE_HEADER:
        obu_payload_size = read_sequence_header_obu(pbi, &rb);
        break;
      case OBU_FRAME_HEADER:
        // Only decode first frame header received
        if (!frame_header_received) {
          frame_header_size =
              read_frame_header_obu(pbi, data, data_end, p_data_end);
          frame_header_received = 1;
        }
        obu_payload_size = frame_header_size;
        if (cm->show_existing_frame) frame_decoding_finished = 1;
        break;
      case OBU_TILE_GROUP:
        obu_payload_size =
            read_one_tile_group_obu(pbi, &rb, is_first_tg_obu_received, data,
                                    data + obu_size - obu_header_size,
                                    p_data_end, &frame_decoding_finished);
        is_first_tg_obu_received = 0;
        break;
      case OBU_METADATA:
        obu_payload_size = read_metadata(data, obu_size);
        break;
      default: break;
    }
    data += obu_payload_size;
    if (data_end < data) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
  }
}
