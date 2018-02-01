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

#if CONFIG_SCALABILITY
// picture prediction structures (0-12 are predefined) in scaiability metadata
typedef enum {
  SCALABILITY_L1T2 = 0,
  SCALABILITY_L1T3 = 1,
  SCALABILITY_L2T1 = 2,
  SCALABILITY_L2T2 = 3,
  SCALABILITY_L2T3 = 4,
  SCALABILITY_S2T1 = 5,
  SCALABILITY_S2T2 = 6,
  SCALABILITY_S2T3 = 7,
  SCALABILITY_L2T2h = 8,
  SCALABILITY_L2T3h = 9,
  SCALABILITY_S2T1h = 10,
  SCALABILITY_S2T2h = 11,
  SCALABILITY_S2T3h = 12,
  SCALABILITY_SS = 13
} SCALABILITY_STRUCTURES;
#endif

static OBU_TYPE read_obu_header(struct aom_read_bit_buffer *rb,
                                size_t *header_size,
                                uint8_t *obu_extension_header) {
  *header_size = 1;

  // first bit is obu_forbidden_bit (0) according to R19
  aom_rb_read_bit(rb);

  const OBU_TYPE obu_type = (OBU_TYPE)aom_rb_read_literal(rb, 4);
  aom_rb_read_literal(rb, 2);  // reserved
  const int obu_extension_flag = aom_rb_read_bit(rb);
  if (obu_extension_header) *obu_extension_header = 0;
  if (obu_extension_flag) {
    *header_size += 1;
#if !CONFIG_SCALABILITY
    aom_rb_read_literal(rb, 3);  // temporal_id
    aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 1);  // reserved
#else
    *obu_extension_header = (uint8_t)aom_rb_read_literal(rb, 8);
#endif
  }

  return obu_type;
}

static uint32_t read_temporal_delimiter_obu() { return 0; }

static uint32_t read_sequence_header_obu(AV1Decoder *pbi,
                                         struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t saved_bit_offset = rb->bit_offset;
#if CONFIG_SCALABILITY
  int i;
#endif

  cm->profile = av1_read_profile(rb);
  aom_rb_read_literal(rb, 4);  // level

#if CONFIG_SCALABILITY
  pbi->common.enhancement_layers_cnt = aom_rb_read_literal(rb, 2);
  for (i = 1; i <= pbi->common.enhancement_layers_cnt; i++) {
    aom_rb_read_literal(rb, 4);  // level for each enhancement layer
  }
#endif

  read_sequence_header(&cm->seq_params, rb);

  av1_read_bitdepth_colorspace_sampling(cm, rb, pbi->allow_lowbitdepth);

#if CONFIG_TIMING_INFO_IN_SEQ_HEADERS
  av1_read_timing_info_header(cm, rb);
#endif

#if CONFIG_FILM_GRAIN
  cm->film_grain_params_present = aom_rb_read_bit(rb);
#endif

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

#if CONFIG_SCALABILITY
static void scalability_structure(struct aom_read_bit_buffer *rb) {
  int enhancement_layers_cnt = aom_rb_read_literal(rb, 2);
  int enhancement_layer_dimensions_present_flag = aom_rb_read_literal(rb, 1);
  int enhancement_layer_description_present_flag = aom_rb_read_literal(rb, 1);
  int temporal_group_description_flag = aom_rb_read_literal(rb, 1);
  aom_rb_read_literal(rb, 3);  // reserved

  if (enhancement_layer_dimensions_present_flag) {
    int i;
    for (i = 0; i < enhancement_layers_cnt + 1; i++) {
      aom_rb_read_literal(rb, 16);
      aom_rb_read_literal(rb, 16);
    }
  }
  if (enhancement_layer_description_present_flag) {
    int i;
    for (i = 0; i < enhancement_layers_cnt + 1; i++) {
      aom_rb_read_literal(rb, 8);
    }
  }
  if (temporal_group_description_flag) {
    int i, j, temporal_group_size;
    temporal_group_size = aom_rb_read_literal(rb, 8);
    for (i = 0; i < temporal_group_size; i++) {
      aom_rb_read_literal(rb, 3);
      aom_rb_read_literal(rb, 1);
      int temporal_group_ref_cnt = aom_rb_read_literal(rb, 2);
      aom_rb_read_literal(rb, 2);
      for (j = 0; j < temporal_group_ref_cnt; j++) {
        aom_rb_read_literal(rb, 8);
      }
    }
  }
}

static void read_metadata_scalability(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  int scalability_mode_idc = aom_rb_read_literal(&rb, 8);
  if (scalability_mode_idc == SCALABILITY_SS) {
    scalability_structure(&rb);
  }
}
#endif

static size_t read_metadata(const uint8_t *data, size_t sz) {
  assert(sz >= 2);
  const OBU_METADATA_TYPE metadata_type = (OBU_METADATA_TYPE)mem_get_le16(data);

  if (metadata_type == OBU_METADATA_TYPE_PRIVATE_DATA) {
    read_metadata_private_data(data + 2, sz - 2);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_CLL) {
    read_metadata_hdr_cll(data + 2);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_MDCV) {
    read_metadata_hdr_mdcv(data + 2);
#if CONFIG_SCALABILITY
  } else if (metadata_type == OBU_METADATA_TYPE_SCALABILITY) {
    read_metadata_scalability(data + 2, sz - 2);
#endif
  }

  return sz;
}

#if CONFIG_SCALABILITY
static const uint32_t kObuExtTemporalIdBitsMask = 0x7;
static const uint32_t kObuExtTemporalIdBitsShift = 5;
static const uint32_t kObuExtEnhancementIdBitsMask = 0x3;
static const uint32_t kObuExtEnhancementIdBitsShift = 3;
#endif

void av1_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
                                const uint8_t *data_end,
                                const uint8_t **p_data_end) {
  AV1_COMMON *const cm = &pbi->common;
  int frame_decoding_finished = 0;
  int is_first_tg_obu_received = 1;
  int frame_header_received = 0;
  int frame_header_size = 0;
#if CONFIG_SCALABILITY
  uint8_t obu_extension_header = 0;
#endif

  if (data_end < data) {
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return;
  }

  // decode frame as a series of OBUs
  while (!frame_decoding_finished && !cm->error.error_code) {
    struct aom_read_bit_buffer rb;
    size_t obu_header_size, obu_payload_size = 0;
    const size_t bytes_available = data_end - data;

    if (bytes_available < 1) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

#if CONFIG_OBU_SIZING
    // OBUs are preceded by an unsigned leb128 coded unsigned integer.
    uint32_t u_obu_size = 0;
    aom_uleb_decode(data, bytes_available, &u_obu_size);
    const size_t obu_size = (size_t)u_obu_size;
    const size_t length_field_size = aom_uleb_size_in_bytes(u_obu_size);
#else
    // every obu is preceded by PRE_OBU_SIZE_BYTES-byte size of obu (obu header
    // + payload size)
    // The obu size is only needed for tile group OBUs
    const size_t obu_size = mem_get_le32(data);
    const size_t length_field_size = PRE_OBU_SIZE_BYTES;
#endif  // CONFIG_OBU_SIZING

    av1_init_read_bit_buffer(pbi, &rb, data + length_field_size, data_end);

#if !CONFIG_SCALABILITY
    const OBU_TYPE obu_type = read_obu_header(&rb, &obu_header_size, NULL);
#else
    const OBU_TYPE obu_type =
        read_obu_header(&rb, &obu_header_size, &obu_extension_header);
#endif

    data += length_field_size + obu_header_size;
    if (data_end < data) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

#if CONFIG_SCALABILITY
    cm->temporal_layer_id =
        (obu_extension_header >> kObuExtTemporalIdBitsShift) &
        kObuExtTemporalIdBitsMask;
    cm->enhancement_layer_id =
        (obu_extension_header >> kObuExtEnhancementIdBitsShift) &
        kObuExtEnhancementIdBitsMask;
#endif

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
