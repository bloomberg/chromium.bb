// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP9 bitstream parser. The main
// purpose of this parser is to support hardware decode acceleration. Some
// accelerators, e.g. libva which implements VA-API, require the caller
// (chrome) to feed them parsed VP9 frame header.
//
// Example usage:
//   {
//     Vp9Parser parser;
//     uint8_t* frame_stream;
//     size_t frame_size;
//
//     // Get frames from, say, WebM parser or IVF parser.
//     while (GetVp9Frame(&frame_stream, &frame_size)) {
//       Vp9FrameHeader header;
//       if (!parser.ParseFrame(frame_stream, frame_size, &header)) {
//         // Parse failed.
//         return false;
//       }
//       // Got a frame parsed successfully.
//     }
//  }

#ifndef MEDIA_FILTERS_VP9_PARSER_H_
#define MEDIA_FILTERS_VP9_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/filters/vp9_raw_bits_reader.h"

namespace media {

const int kVp9MaxProfile = 4;
const int kVp9NumRefFramesLog2 = 3;
const int kVp9NumRefFrames = 1 << kVp9NumRefFramesLog2;
const uint8_t kVp9MaxProb = 255;
const int kVp9NumRefsPerFrame = 3;

enum class Vp9ColorSpace {
  UNKNOWN = 0,
  BT_601 = 1,
  BT_709 = 2,
  SMPTE_170 = 3,
  SMPTE_240 = 4,
  BT_2020 = 5,
  RESERVED = 6,
  SRGB = 7,
};

enum class Vp9InterpFilter {
  INTERP_FILTER_SELECT = 0,
  EIGHTTAP_SMOOTH = 1,
  EIGHTTAP = 2,
  EIGHTTAP_SHARP = 3,
  BILINEAR = 4,
};

// Members of Vp9FrameHeader will be 0-initialized by Vp9Parser::ParseFrame.
struct MEDIA_EXPORT Vp9Segmentation {
  static const int kNumSegments = 8;
  static const int kNumTreeProbs = kNumSegments - 1;
  static const int kNumPredictionProbs = 3;
  static const int kNumFeatures = 4;

  bool enabled;

  bool update_map;
  uint8_t tree_probs[kNumTreeProbs];
  bool temporal_update;
  uint8_t pred_probs[kNumPredictionProbs];

  bool update_data;
  bool abs_delta;
  bool feature_enabled[kNumSegments][kNumFeatures];
  int8_t feature_data[kNumSegments][kNumFeatures];
};

// Members of Vp9FrameHeader will be 0-initialized by Vp9Parser::ParseFrame.
struct MEDIA_EXPORT Vp9LoopFilter {
  static const int kNumRefDeltas = 4;
  static const int kNumModeDeltas = 2;

  uint8_t filter_level;
  uint8_t sharpness_level;

  bool mode_ref_delta_enabled;
  bool mode_ref_delta_update;
  bool update_ref_deltas[kNumRefDeltas];
  int8_t ref_deltas[kNumRefDeltas];
  bool update_mode_deltas[kNumModeDeltas];
  int8_t mode_deltas[kNumModeDeltas];
};

// Members of Vp9FrameHeader will be 0-initialized by Vp9Parser::ParseFrame.
struct MEDIA_EXPORT Vp9QuantizationParams {
  bool IsLossless() const {
    return base_qindex == 0 && y_dc_delta == 0 && uv_dc_delta == 0 &&
           uv_ac_delta == 0;
  }

  uint8_t base_qindex;
  int8_t y_dc_delta;
  int8_t uv_dc_delta;
  int8_t uv_ac_delta;
};

// VP9 frame header.
struct MEDIA_EXPORT Vp9FrameHeader {
  enum FrameType {
    KEYFRAME = 0,
    INTERFRAME = 1,
  };

  bool IsKeyframe() const { return frame_type == KEYFRAME; }

  uint8_t profile;

  bool show_existing_frame;
  uint8_t frame_to_show;

  FrameType frame_type;

  bool show_frame;
  bool error_resilient_mode;

  uint8_t bit_depth;
  Vp9ColorSpace color_space;
  bool yuv_range;
  uint8_t subsampling_x;
  uint8_t subsampling_y;

  // The range of width and height is 1..2^16.
  uint32_t width;
  uint32_t height;
  uint32_t display_width;
  uint32_t display_height;

  bool intra_only;
  uint8_t reset_context;
  bool refresh_flag[kVp9NumRefFrames];
  uint8_t frame_refs[kVp9NumRefsPerFrame];
  bool ref_sign_biases[kVp9NumRefsPerFrame];
  bool allow_high_precision_mv;
  Vp9InterpFilter interp_filter;

  bool refresh_frame_context;
  bool frame_parallel_decoding_mode;
  uint8_t frame_context_idx;

  Vp9LoopFilter loop_filter;
  Vp9QuantizationParams quant_params;
  Vp9Segmentation segment;

  uint8_t log2_tile_cols;
  uint8_t log2_tile_rows;

  // Size of compressed header in bytes.
  size_t first_partition_size;

  // Size of uncompressed header in bytes.
  size_t uncompressed_header_size;
};

// A parser for VP9 bitstream.
class MEDIA_EXPORT Vp9Parser {
 public:
  Vp9Parser();

  // Parses one frame and fills parsing result to |fhdr|. Returns true on
  // success, false otherwise.
  // |stream| is the address of VP9 bitstream with |size|.
  bool ParseFrame(const uint8_t* stream, size_t size, Vp9FrameHeader* fhdr);

 private:
  // The parsing context to keep track of references.
  struct ReferenceSlot {
    uint32_t width;
    uint32_t height;
  };

  uint8_t ReadProfile();
  bool VerifySyncCode();
  bool ReadBitDepthColorSpaceSampling(Vp9FrameHeader* fhdr);
  void ReadFrameSize(Vp9FrameHeader* fhdr);
  bool ReadFrameSizeFromRefs(Vp9FrameHeader* fhdr);
  void ReadDisplayFrameSize(Vp9FrameHeader* fhdr);
  Vp9InterpFilter ReadInterpFilter();
  void ReadLoopFilter(Vp9LoopFilter* loop_filter);
  void ReadQuantization(Vp9QuantizationParams* quants);
  void ReadSegmentationMap(Vp9Segmentation* segment);
  void ReadSegmentationData(Vp9Segmentation* segment);
  void ReadSegmentation(Vp9Segmentation* segment);
  void ReadTiles(Vp9FrameHeader* fhdr);
  bool ParseUncompressedHeader(Vp9FrameHeader* fhdr);
  void UpdateSlots(const Vp9FrameHeader* fhdr);

  // Start address of VP9 bitstream buffer.
  const uint8_t* stream_;

  // Size of |stream_| in bytes.
  size_t size_;

  // Raw bits decoder for uncompressed frame header.
  Vp9RawBitsReader reader_;

  // The parsing context to keep track of references.
  ReferenceSlot ref_slots_[kVp9NumRefFrames];

  DISALLOW_COPY_AND_ASSIGN(Vp9Parser);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VP9_PARSER_H_
