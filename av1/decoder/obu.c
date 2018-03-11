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
// Picture prediction structures (0-12 are predefined) in scalability metadata.
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

typedef struct {
  size_t size;
  OBU_TYPE type;
  int has_extension;
  int temporal_layer_id;
  int enhancement_layer_id;
} ObuHeader;

int get_obu_type(uint8_t obu_header, OBU_TYPE *obu_type) {
  if (!obu_type) return -1;
  *obu_type = (OBU_TYPE)((obu_header >> 3) & 0xF);
  switch (*obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_TILE_GROUP:
    case OBU_METADATA:
    case OBU_PADDING: break;
    default: return -1;
  }
  return 0;
}

// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type) {
  int valid_type = 0;
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_TILE_GROUP:
    case OBU_FRAME:
    case OBU_METADATA:
    case OBU_PADDING: valid_type = 1; break;
    default: break;
  }
  return valid_type;
}

// Parses OBU header and stores values in 'header'. Returns 0 for success, and
// -1 when an error occurs.
static int read_obu_header(struct aom_read_bit_buffer *rb, ObuHeader *header) {
  if (!rb || !header) return AOM_CODEC_INVALID_PARAM;

  header->size = 1;

  // first bit is obu_forbidden_bit (0) according to R19
  aom_rb_read_bit(rb);

  header->type = (OBU_TYPE)aom_rb_read_literal(rb, 4);

  if (!valid_obu_type(header->type)) return AOM_CODEC_CORRUPT_FRAME;

#if CONFIG_OBU_SIZE_AFTER_HEADER
  header->has_extension = aom_rb_read_bit(rb);
  // TODO(vigneshv): This field is ignored for now since the library always
  // assumes that length is going to be present after the obu header when this
  // experiment is on.
  aom_rb_read_bit(rb);  // obu_has_payload_length_field
  aom_rb_read_bit(rb);  // reserved
#else
  aom_rb_read_literal(rb, 2);  // reserved
  header->has_extension = aom_rb_read_bit(rb);
#endif

  if (header->has_extension) {
    header->size += 1;
    header->temporal_layer_id = aom_rb_read_literal(rb, 3);
    header->enhancement_layer_id = aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 3);  // reserved
  }

  return AOM_CODEC_OK;
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

  av1_read_timing_info_header(cm, rb);

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
  int tile_start_and_end_present_flag = 0;
  const int num_tiles = pbi->common.tile_rows * pbi->common.tile_cols;

  if (!pbi->common.large_scale_tile && num_tiles > 1) {
    tile_start_and_end_present_flag = aom_rb_read_bit(rb);
  }
  if (pbi->common.large_scale_tile || num_tiles == 1 ||
      !tile_start_and_end_present_flag) {
    *startTile = 0;
    *endTile = num_tiles - 1;
    return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
  }

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

#if CONFIG_OBU_SIZING
static aom_codec_err_t read_obu_size(const uint8_t *data,
                                     size_t bytes_available,
                                     size_t *const obu_size,
                                     size_t *const length_field_size) {
  uint64_t u_obu_size = 0;
  if (aom_uleb_decode(data, bytes_available, &u_obu_size, length_field_size) !=
      0) {
    return AOM_CODEC_CORRUPT_FRAME;
  }

  *obu_size = (size_t)u_obu_size;
  return AOM_CODEC_OK;
}
#endif  // CONFIG_OBU_SIZING

void av1_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
                                const uint8_t *data_end,
                                const uint8_t **p_data_end) {
  AV1_COMMON *const cm = &pbi->common;
  int frame_decoding_finished = 0;
  int is_first_tg_obu_received = 1;
  int frame_header_received = 0;
  int frame_header_size = 0;
  int seq_header_received = 0;
  size_t seq_header_size = 0;
  ObuHeader obu_header;
  memset(&obu_header, 0, sizeof(obu_header));

  if (data_end < data) {
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return;
  }

  // decode frame as a series of OBUs
  while (!frame_decoding_finished && !cm->error.error_code) {
    struct aom_read_bit_buffer rb;
    size_t payload_size = 0;
    size_t decoded_payload_size = 0;
    size_t obu_payload_offset = 0;
    const size_t bytes_available = data_end - data;

    if (bytes_available < 1) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

#if CONFIG_OBU_SIZE_AFTER_HEADER
    size_t length_field_size = 0;
#elif CONFIG_OBU_SIZING
    size_t length_field_size;
    size_t obu_size;
    if (read_obu_size(data, bytes_available, &obu_size, &length_field_size) !=
        AOM_CODEC_OK) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
#else
    // every obu is preceded by PRE_OBU_SIZE_BYTES-byte size of obu (obu header
    // + payload size)
    // The obu size is only needed for tile group OBUs
    const size_t obu_size = mem_get_le32(data);
    const size_t length_field_size = PRE_OBU_SIZE_BYTES;
    if (obu_size > bytes_available) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
#endif  // CONFIG_OBU_SIZE_AFTER_HEADER

    if (data_end < data + length_field_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
    av1_init_read_bit_buffer(pbi, &rb, data + length_field_size, data_end);

    if (read_obu_header(&rb, &obu_header) != AOM_CODEC_OK) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

#if CONFIG_OBU_SIZE_AFTER_HEADER
    if (read_obu_size(data + obu_header.size, bytes_available - obu_header.size,
                      &payload_size, &length_field_size) != AOM_CODEC_OK) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
    av1_init_read_bit_buffer(
        pbi, &rb, data + length_field_size + obu_header.size, data_end);
#else
    payload_size = obu_size - obu_header.size;
#endif  // CONFIG_OBU_SIZE_AFTER_HEADER

    data += length_field_size + obu_header.size;
    if (data_end < data) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

#if CONFIG_SCALABILITY
    cm->temporal_layer_id = obu_header.temporal_layer_id;
    cm->enhancement_layer_id = obu_header.enhancement_layer_id;
#endif

    switch (obu_header.type) {
      case OBU_TEMPORAL_DELIMITER:
        decoded_payload_size = read_temporal_delimiter_obu();
        break;
      case OBU_SEQUENCE_HEADER:
        if (!seq_header_received) {
          decoded_payload_size = read_sequence_header_obu(pbi, &rb);
          seq_header_size = decoded_payload_size;
          seq_header_received = 1;
        } else {
          // Seeing another sequence header, skip as all sequence headers
          // are requred to be identical.
          if (payload_size != seq_header_size) {
            cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
            return;
          }
          decoded_payload_size = seq_header_size;
        }
        break;
#if CONFIG_OBU_FRAME
      case OBU_FRAME:
#endif  // CONFIG_OBU_FRAME
      case OBU_FRAME_HEADER:
        // Only decode first frame header received
        if (!frame_header_received) {
          frame_header_size =
              read_frame_header_obu(pbi, data, data_end, p_data_end);
          frame_header_received = 1;
        }
        decoded_payload_size = frame_header_size;
        if (cm->show_existing_frame) {
          frame_decoding_finished = 1;
          break;
        }
#if CONFIG_OBU_FRAME
        if (obu_header.type == OBU_FRAME_HEADER) break;
        obu_payload_offset = frame_header_size;
        AOM_FALLTHROUGH_INTENDED;  // fall through to read tile group.
#else
        break;
#endif  // CONFIG_OBU_FRAME
      case OBU_TILE_GROUP:
        if (!frame_header_received) {
          cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
          return;
        }
        if (data_end < data + obu_payload_offset ||
            data_end < data + payload_size) {
          cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
          return;
        }
        decoded_payload_size += read_one_tile_group_obu(
            pbi, &rb, is_first_tg_obu_received, data + obu_payload_offset,
            data + payload_size, p_data_end, &frame_decoding_finished);
        is_first_tg_obu_received = 0;
        break;
      case OBU_METADATA:
        decoded_payload_size = read_metadata(data, payload_size);
        break;
      case OBU_PADDING:
      default:
        // Skip unrecognized OBUs
        decoded_payload_size = payload_size;
        break;
    }

    // Check that the signalled OBU size matches the actual amount of data read
    if (decoded_payload_size != payload_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

    data += payload_size;
    if (data_end < data) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }
  }
}
