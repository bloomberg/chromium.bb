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
#include "av1/decoder/obu.h"

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
  SCALABILITY_L2T1h = 8,
  SCALABILITY_L2T2h = 9,
  SCALABILITY_L2T3h = 10,
  SCALABILITY_S2T1h = 11,
  SCALABILITY_S2T2h = 12,
  SCALABILITY_S2T3h = 13,
  SCALABILITY_SS = 14
} SCALABILITY_STRUCTURES;

// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type) {
  int valid_type = 0;
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_REDUNDANT_FRAME_HEADER:
    case OBU_FRAME:
    case OBU_TILE_GROUP:
    case OBU_METADATA:
    case OBU_PADDING: valid_type = 1; break;
    default: break;
  }
  return valid_type;
}

// Parses OBU header and stores values in 'header'.
static aom_codec_err_t read_obu_header(struct aom_read_bit_buffer *rb,
                                       int is_annexb, ObuHeader *header) {
  if (!rb || !header) return AOM_CODEC_INVALID_PARAM;

  const ptrdiff_t bit_buffer_byte_length = rb->bit_buffer_end - rb->bit_buffer;
  if (bit_buffer_byte_length < 1) return AOM_CODEC_CORRUPT_FRAME;

  header->size = 1;

  if (aom_rb_read_bit(rb) != 0) {
    // Forbidden bit. Must not be set.
    return AOM_CODEC_CORRUPT_FRAME;
  }

  header->type = (OBU_TYPE)aom_rb_read_literal(rb, 4);

  if (!valid_obu_type(header->type)) return AOM_CODEC_CORRUPT_FRAME;

  header->has_extension = aom_rb_read_bit(rb);
  header->has_length_field = aom_rb_read_bit(rb);

  if (!header->has_length_field && !is_annexb) {
    // section 5 obu streams must have length field set.
    return AOM_CODEC_UNSUP_BITSTREAM;
  }

  aom_rb_read_bit(rb);  // reserved

  if (header->has_extension) {
    if (bit_buffer_byte_length == 1) return AOM_CODEC_CORRUPT_FRAME;

    header->size += 1;
    header->temporal_layer_id = aom_rb_read_literal(rb, 3);
    header->spatial_layer_id = aom_rb_read_literal(rb, 2);
    aom_rb_read_literal(rb, 3);  // reserved
  }

  return AOM_CODEC_OK;
}

aom_codec_err_t aom_read_obu_header(uint8_t *buffer, size_t buffer_length,
                                    size_t *consumed, ObuHeader *header,
                                    int is_annexb) {
  if (buffer_length < 1 || !consumed || !header) return AOM_CODEC_INVALID_PARAM;

  // TODO(tomfinegan): Set the error handler here and throughout this file, and
  // confirm parsing work done via aom_read_bit_buffer is successful.
  struct aom_read_bit_buffer rb = { buffer, buffer + buffer_length, 0, NULL,
                                    NULL };
  aom_codec_err_t parse_result = read_obu_header(&rb, is_annexb, header);
  if (parse_result == AOM_CODEC_OK) *consumed = header->size;
  return parse_result;
}

int aom_get_num_layers_from_operating_point_idc(
    int operating_point_idc, unsigned int *number_spatial_layers,
    unsigned int *number_temporal_layers) {
  // derive number of spatial/temporal layers from operating_point_idc

  if (!number_spatial_layers || !number_temporal_layers) return -1;
  if (operating_point_idc == 0) {
    *number_temporal_layers = 1;
    *number_spatial_layers = 1;
  } else {
    *number_spatial_layers = 0;
    *number_temporal_layers = 0;
    for (int j = 0; j < MAX_NUM_SPATIAL_LAYERS; j++) {
      *number_spatial_layers +=
          (operating_point_idc >> (j + MAX_NUM_TEMPORAL_LAYERS)) & 0x1;
    }
    for (int j = 0; j < MAX_NUM_TEMPORAL_LAYERS; j++) {
      *number_temporal_layers += (operating_point_idc >> j) & 0x1;
    }
  }

  if (*number_spatial_layers > MAX_NUM_SPATIAL_LAYERS ||
      *number_temporal_layers > MAX_NUM_TEMPORAL_LAYERS)
    return -1;

  return 0;
}

static int is_obu_in_current_operating_point(AV1Decoder *pbi,
                                             ObuHeader obu_header) {
  if (!pbi->current_operating_point) {
    return 1;
  }

  if ((pbi->current_operating_point >> obu_header.temporal_layer_id) & 0x1 &&
      (pbi->current_operating_point >> (obu_header.spatial_layer_id + 8)) &
          0x1) {
    return 1;
  }
  return 0;
}

static uint32_t read_temporal_delimiter_obu() { return 0; }

static int read_bitstream_level(BitstreamLevel *bl,
                                struct aom_read_bit_buffer *rb) {
  const uint8_t seq_level_idx = aom_rb_read_literal(rb, LEVEL_BITS);
  if (!is_valid_seq_level_idx(seq_level_idx)) return 0;
  bl->major = (seq_level_idx >> LEVEL_MINOR_BITS) + LEVEL_MAJOR_MIN;
  bl->minor = seq_level_idx & ((1 << LEVEL_MINOR_BITS) - 1);
  return 1;
}

static uint32_t read_sequence_header_obu(AV1Decoder *pbi,
                                         struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t saved_bit_offset = rb->bit_offset;
  int operating_points_cnt_minus_1 = 0;

  cm->profile = av1_read_profile(rb);

  SequenceHeader *seq_params = &cm->seq_params;

  // Still picture or not
  seq_params->still_picture = aom_rb_read_bit(rb);
  seq_params->reduced_still_picture_hdr = aom_rb_read_bit(rb);
  // Video must have reduced_still_picture_hdr = 0
  if (!cm->seq_params.still_picture &&
      cm->seq_params.reduced_still_picture_hdr) {
    return AOM_CODEC_UNSUP_BITSTREAM;
  }

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->operating_point_idc[0] = 0;
    if (!read_bitstream_level(&seq_params->level[0], rb))
      return AOM_CODEC_UNSUP_BITSTREAM;
    seq_params->decoder_rate_model_param_present_flag[0] = 0;
  } else {
    operating_points_cnt_minus_1 =
        aom_rb_read_literal(rb, OP_POINTS_CNT_MINUS_1_BITS);
    for (int i = 0; i < operating_points_cnt_minus_1 + 1; i++) {
      seq_params->operating_point_idc[i] =
          aom_rb_read_literal(rb, OP_POINTS_IDC_BITS);
      if (!read_bitstream_level(&seq_params->level[i], rb))
        return AOM_CODEC_UNSUP_BITSTREAM;
      if (seq_params->level[i].major > 3)
        seq_params->tier[i] = aom_rb_read_bit(rb);
      else
        seq_params->tier[i] = 0;
#if !CONFIG_BUFFER_MODEL
      seq_params->decoder_rate_model_param_present_flag[i] =
          aom_rb_read_literal(rb, 1);
      if (seq_params->decoder_rate_model_param_present_flag[i]) {
        seq_params->decode_to_display_rate_ratio[i] =
            aom_rb_read_literal(rb, 12);
        seq_params->initial_display_delay[i] = aom_rb_read_literal(rb, 24);
        seq_params->extra_frame_buffers[i] = aom_rb_read_literal(rb, 4);
      }
#endif
    }
  }
  // This decoder supports all levels.  Choose operating point provided by
  // external means
  int operating_point = pbi->operating_point;
  if (operating_point < 0 || operating_point > operating_points_cnt_minus_1)
    operating_point = 0;
  pbi->current_operating_point =
      seq_params->operating_point_idc[operating_point];
  aom_get_num_layers_from_operating_point_idc(pbi->current_operating_point,
                                              &cm->number_spatial_layers,
                                              &cm->number_temporal_layers);

  read_sequence_header(cm, rb);

  av1_read_bitdepth_colorspace_sampling(cm, rb, pbi->allow_lowbitdepth);

#if !CONFIG_BUFFER_MODEL
  if (!seq_params->reduced_still_picture_hdr)
    av1_read_timing_info_header(cm, rb);
  else
    cm->timing_info_present = 0;
#else
  if (!seq_params->reduced_still_picture_hdr)
    cm->timing_info_present = aom_rb_read_bit(rb);  // timing info present flag
  else
    cm->timing_info_present = 0;

  if (cm->timing_info_present) {
    av1_read_timing_info_header(cm, rb);

    cm->decoder_model_info_present_flag = aom_rb_read_bit(rb);
    if (cm->decoder_model_info_present_flag)
      av1_read_decoder_model_info(cm, rb);
  } else {
    cm->decoder_model_info_present_flag = 0;
  }
  int operating_points_decoder_model_present = aom_rb_read_bit(rb);
  if (operating_points_decoder_model_present) {
    cm->operating_points_decoder_model_cnt = aom_rb_read_literal(rb, 5) + 1;
  } else {
    cm->operating_points_decoder_model_cnt = 0;
  }
  for (int op_num = 0; op_num < cm->operating_points_decoder_model_cnt;
       ++op_num) {
    cm->op_params[op_num].decoder_model_operating_point_idc =
        aom_rb_read_literal(rb, 12);
    cm->op_params[op_num].display_model_param_present_flag =
        aom_rb_read_bit(rb);
    if (cm->op_params[op_num].display_model_param_present_flag) {
      cm->op_params[op_num].initial_display_delay =
          aom_rb_read_literal(rb, 4) + 1;
      if (cm->op_params[op_num].initial_display_delay > 10)
        aom_internal_error(
            &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
            "AV1 does not support more than 10 decoded frames delay");
    }
    if (cm->decoder_model_info_present_flag) {
      cm->op_params[op_num].decoder_model_param_present_flag =
          aom_rb_read_bit(rb);
      if (cm->op_params[op_num].decoder_model_param_present_flag)
        av1_read_op_parameters_info(cm, rb, op_num);
    }
  }
#endif

  cm->film_grain_params_present = aom_rb_read_bit(rb);

  av1_check_trailing_bits(pbi, rb);

  pbi->sequence_header_ready = 1;

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_frame_header_obu(AV1Decoder *pbi,
                                      struct aom_read_bit_buffer *rb,
                                      const uint8_t *data,
                                      const uint8_t **p_data_end,
                                      int trailing_bits_present) {
  av1_decode_frame_headers_and_setup(pbi, rb, data, p_data_end,
                                     trailing_bits_present);
  return (uint32_t)(pbi->uncomp_hdr_size);
}

static int32_t read_tile_group_header(AV1Decoder *pbi,
                                      struct aom_read_bit_buffer *rb,
                                      int *startTile, int *endTile,
                                      int tile_start_implicit) {
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
  if (tile_start_implicit && tile_start_and_end_present_flag) {
    aom_internal_error(
        &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
        "For OBU_FRAME type obu tile_start_and_end_present_flag must be 0");
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return -1;
  }
  *startTile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);
  *endTile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_one_tile_group_obu(
    AV1Decoder *pbi, struct aom_read_bit_buffer *rb, int is_first_tg,
    const uint8_t *data, const uint8_t *data_end, const uint8_t **p_data_end,
    int *is_last_tg, int tile_start_implicit) {
  AV1_COMMON *const cm = &pbi->common;
  int startTile, endTile;
  int32_t header_size, tg_payload_size;

  header_size = read_tile_group_header(pbi, rb, &startTile, &endTile,
                                       tile_start_implicit);
  if (header_size == -1) return 0;
  if (startTile > endTile) return header_size;
  data += header_size;
  av1_decode_tg_tiles_and_wrapup(pbi, data, data_end, p_data_end, startTile,
                                 endTile, is_first_tg);

  tg_payload_size = (uint32_t)(*p_data_end - data);

  // TODO(shan):  For now, assume all tile groups received in order
  *is_last_tg = endTile == cm->tile_rows * cm->tile_cols - 1;
  return header_size + tg_payload_size;
}

static void read_metadata_itut_t35(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  for (size_t i = 0; i < sz; i++) {
    aom_rb_read_literal(&rb, 8);
  }
}

static void read_metadata_hdr_cll(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  aom_rb_read_literal(&rb, 16);  // max_cll
  aom_rb_read_literal(&rb, 16);  // max_fall
}

static void read_metadata_hdr_mdcv(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  for (int i = 0; i < 3; i++) {
    aom_rb_read_literal(&rb, 16);  // primary_i_chromaticity_x
    aom_rb_read_literal(&rb, 16);  // primary_i_chromaticity_y
  }

  aom_rb_read_literal(&rb, 16);  // white_point_chromaticity_x
  aom_rb_read_literal(&rb, 16);  // white_point_chromaticity_y

  aom_rb_read_unsigned_literal(&rb, 32);  // luminance_max
  aom_rb_read_unsigned_literal(&rb, 32);  // luminance_min
}

static void scalability_structure(struct aom_read_bit_buffer *rb) {
  int spatial_layers_cnt = aom_rb_read_literal(rb, 2);
  int spatial_layer_dimensions_present_flag = aom_rb_read_literal(rb, 1);
  int spatial_layer_description_present_flag = aom_rb_read_literal(rb, 1);
  int temporal_group_description_present_flag = aom_rb_read_literal(rb, 1);
  aom_rb_read_literal(rb, 3);  // reserved

  if (spatial_layer_dimensions_present_flag) {
    int i;
    for (i = 0; i < spatial_layers_cnt + 1; i++) {
      aom_rb_read_literal(rb, 16);
      aom_rb_read_literal(rb, 16);
    }
  }
  if (spatial_layer_description_present_flag) {
    int i;
    for (i = 0; i < spatial_layers_cnt + 1; i++) {
      aom_rb_read_literal(rb, 8);
    }
  }
  if (temporal_group_description_present_flag) {
    int i, j, temporal_group_size;
    temporal_group_size = aom_rb_read_literal(rb, 8);
    for (i = 0; i < temporal_group_size; i++) {
      aom_rb_read_literal(rb, 3);
      aom_rb_read_literal(rb, 1);
      aom_rb_read_literal(rb, 1);
      int temporal_group_ref_cnt = aom_rb_read_literal(rb, 3);
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

static void read_metadata_timecode(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  aom_rb_read_literal(&rb, 5);                     // counting_type f(5)
  int full_timestamp_flag = aom_rb_read_bit(&rb);  // full_timestamp_flag f(1)
  aom_rb_read_bit(&rb);                            // discontinuity_flag (f1)
  aom_rb_read_bit(&rb);                            // cnt_dropped_flag f(1)
  aom_rb_read_literal(&rb, 9);                     // n_frames f(9)
  if (full_timestamp_flag) {
    aom_rb_read_literal(&rb, 6);  // seconds_value f(6)
    aom_rb_read_literal(&rb, 6);  // minutes_value f(6)
    aom_rb_read_literal(&rb, 5);  // hours_value f(5)
  } else {
    int seconds_flag = aom_rb_read_bit(&rb);  // seconds_flag f(1)
    if (seconds_flag) {
      aom_rb_read_literal(&rb, 6);              // seconds_value f(6)
      int minutes_flag = aom_rb_read_bit(&rb);  // minutes_flag f(1)
      if (minutes_flag) {
        aom_rb_read_literal(&rb, 6);            // minutes_value f(6)
        int hours_flag = aom_rb_read_bit(&rb);  // hours_flag f(1)
        if (hours_flag) {
          aom_rb_read_literal(&rb, 5);  // hours_value f(5)
        }
      }
    }
  }
  // time_offset_length f(5)
  int time_offset_length = aom_rb_read_literal(&rb, 5);
  if (time_offset_length) {
    aom_rb_read_literal(&rb, time_offset_length);  // f(time_offset_length)
  }
}

static size_t read_metadata(const uint8_t *data, size_t sz) {
  size_t type_length;
  uint64_t type_value;
  OBU_METADATA_TYPE metadata_type;
  if (aom_uleb_decode(data, sz, &type_value, &type_length) < 0) {
    return sz;
  }
  metadata_type = (OBU_METADATA_TYPE)type_value;
  if (metadata_type == OBU_METADATA_TYPE_ITUT_T35) {
    read_metadata_itut_t35(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_CLL) {
    read_metadata_hdr_cll(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_MDCV) {
    read_metadata_hdr_mdcv(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_SCALABILITY) {
    read_metadata_scalability(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_TIMECODE) {
    read_metadata_timecode(data + type_length, sz - type_length);
  }

  return sz;
}

static aom_codec_err_t read_obu_size(const uint8_t *data,
                                     size_t bytes_available,
                                     size_t *const obu_size,
                                     size_t *const length_field_size) {
  uint64_t u_obu_size = 0;
  if (aom_uleb_decode(data, bytes_available, &u_obu_size, length_field_size) !=
      0) {
    return AOM_CODEC_CORRUPT_FRAME;
  }

  if (u_obu_size > UINT32_MAX) return AOM_CODEC_CORRUPT_FRAME;
  *obu_size = (size_t)u_obu_size;
  return AOM_CODEC_OK;
}

aom_codec_err_t aom_read_obu_header_and_size(const uint8_t *data,
                                             size_t bytes_available,
                                             int is_annexb,
                                             ObuHeader *obu_header,
                                             size_t *const payload_size,
                                             size_t *const bytes_read) {
  size_t length_field_size = 0, obu_size = 0;
  aom_codec_err_t status;

  if (is_annexb) {
    // Size field comes before the OBU header, and includes the OBU header
    status =
        read_obu_size(data, bytes_available, &obu_size, &length_field_size);

    if (status != AOM_CODEC_OK) return status;
  }

  struct aom_read_bit_buffer rb = { data + length_field_size,
                                    data + bytes_available, 0, NULL, NULL };

  status = read_obu_header(&rb, is_annexb, obu_header);
  if (status != AOM_CODEC_OK) return status;

  if (is_annexb) {
    // Derive the payload size from the data we've already read
    if (obu_size < obu_header->size) return AOM_CODEC_CORRUPT_FRAME;

    *payload_size = obu_size - obu_header->size;
  } else {
    // Size field comes after the OBU header, and is just the payload size
    status = read_obu_size(data + obu_header->size,
                           bytes_available - obu_header->size, payload_size,
                           &length_field_size);
    if (status != AOM_CODEC_OK) return status;
  }

  *bytes_read = length_field_size + obu_header->size;
  return AOM_CODEC_OK;
}

void aom_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
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
  pbi->dropped_obus = 0;

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
    size_t bytes_read = 0;
    const size_t bytes_available = data_end - data;

    if (bytes_available == 0 && !frame_header_received) {
      cm->error.error_code = AOM_CODEC_OK;
      return;
    }

    aom_codec_err_t status =
        aom_read_obu_header_and_size(data, bytes_available, cm->is_annexb,
                                     &obu_header, &payload_size, &bytes_read);
    if (status != AOM_CODEC_OK) {
      cm->error.error_code = status;
      return;
    }

    // Note: aom_read_obu_header_and_size() takes care of checking that this
    // doesn't cause 'data' to advance past 'data_end'.
    data += bytes_read;

    cm->temporal_layer_id = obu_header.temporal_layer_id;
    cm->spatial_layer_id = obu_header.spatial_layer_id;

    if (obu_header.type != OBU_TEMPORAL_DELIMITER &&
        obu_header.type != OBU_SEQUENCE_HEADER &&
        obu_header.type != OBU_PADDING) {
      // don't decode obu if it's not in current operating mode
      if (!is_obu_in_current_operating_point(pbi, obu_header)) {
        data += payload_size;
        *p_data_end = data;
        pbi->dropped_obus++;
        continue;
      }
    }

    av1_init_read_bit_buffer(pbi, &rb, data, data_end);

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
          // are required to be identical.
          if (payload_size != seq_header_size) {
            cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
            return;
          }
          decoded_payload_size = seq_header_size;
        }
        break;
      case OBU_FRAME_HEADER:
      case OBU_REDUNDANT_FRAME_HEADER:
      case OBU_FRAME:
        // Only decode first frame header received
        if (!frame_header_received) {
          pbi->dropped_obus = 0;
          av1_init_read_bit_buffer(pbi, &rb, data, data_end);
          frame_header_size = read_frame_header_obu(
              pbi, &rb, data, p_data_end, obu_header.type != OBU_FRAME);
          frame_header_received = 1;
        }
        decoded_payload_size = frame_header_size;
        if (cm->show_existing_frame) {
          frame_decoding_finished = 1;
          break;
        }
        if (obu_header.type != OBU_FRAME) break;
        obu_payload_offset = frame_header_size;
        AOM_FALLTHROUGH_INTENDED;  // fall through to read tile group.
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
            data + payload_size, p_data_end, &frame_decoding_finished,
            obu_header.type == OBU_FRAME);
        is_first_tg_obu_received = 0;
        break;
      case OBU_METADATA:
        if (data_end < data + payload_size) {
          cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
          return;
        }
        decoded_payload_size = read_metadata(data, payload_size);
        break;
      case OBU_PADDING:
      default:
        // Skip unrecognized OBUs
        decoded_payload_size = payload_size;
        break;
    }

    // Check that the signalled OBU size matches the actual amount of data read
    if (decoded_payload_size > payload_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

    if (data_end < data + payload_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return;
    }

    // If there are extra padding bytes, they should all be zero
    while (decoded_payload_size < payload_size) {
      uint8_t padding_byte = data[decoded_payload_size++];
      if (padding_byte != 0) {
        cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
        return;
      }
    }

    data += payload_size;
  }
}
