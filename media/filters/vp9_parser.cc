// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP9 bitstream parser.

#include "media/filters/vp9_parser.h"

#include "base/logging.h"

namespace {

// Helper function for Vp9Parser::ReadTiles. Defined as get_min_log2_tile_cols
// in spec.
int GetMinLog2TileCols(int sb64_cols) {
  const int kMaxTileWidthB64 = 64;
  int min_log2 = 0;
  while ((kMaxTileWidthB64 << min_log2) < sb64_cols)
    min_log2++;
  return min_log2;
}

// Helper function for Vp9Parser::ReadTiles. Defined as get_max_log2_tile_cols
// in spec.
int GetMaxLog2TileCols(int sb64_cols) {
  const int kMinTileWidthB64 = 4;
  int max_log2 = 1;
  while ((sb64_cols >> max_log2) >= kMinTileWidthB64)
    max_log2++;
  return max_log2 - 1;
}

}  // namespace

namespace media {

Vp9Parser::Vp9Parser() : stream_(nullptr), size_(0) {
  memset(&ref_slots_, 0, sizeof(ref_slots_));
}

uint8_t Vp9Parser::ReadProfile() {
  uint8_t profile = 0;

  // LSB first.
  if (reader_.ReadBool())
    profile |= 1;
  if (reader_.ReadBool())
    profile |= 2;
  if (profile > 2 && reader_.ReadBool())
    profile += 1;
  return profile;
}

bool Vp9Parser::VerifySyncCode() {
  const int kSyncCode = 0x498342;
  if (reader_.ReadLiteral(8 * 3) != kSyncCode) {
    DVLOG(1) << "Invalid frame sync code";
    return false;
  }
  return true;
}

bool Vp9Parser::ReadBitDepthColorSpaceSampling(Vp9FrameHeader* fhdr) {
  if (fhdr->profile == 2 || fhdr->profile == 3) {
    fhdr->bit_depth = reader_.ReadBool() ? 12 : 10;
  } else {
    fhdr->bit_depth = 8;
  }

  fhdr->color_space = static_cast<Vp9ColorSpace>(reader_.ReadLiteral(3));
  if (fhdr->color_space != Vp9ColorSpace::SRGB) {
    fhdr->yuv_range = reader_.ReadBool();
    if (fhdr->profile == 1 || fhdr->profile == 3) {
      fhdr->subsampling_x = reader_.ReadBool() ? 1 : 0;
      fhdr->subsampling_y = reader_.ReadBool() ? 1 : 0;
      if (fhdr->subsampling_x == 1 && fhdr->subsampling_y == 1) {
        DVLOG(1) << "4:2:0 color not supported in profile 1 or 3";
        return false;
      }
      bool reserved = reader_.ReadBool();
      if (reserved) {
        DVLOG(1) << "reserved bit set";
        return false;
      }
    } else {
      fhdr->subsampling_x = fhdr->subsampling_y = 1;
    }
  } else {
    if (fhdr->profile == 1 || fhdr->profile == 3) {
      fhdr->subsampling_x = fhdr->subsampling_y = 0;

      bool reserved = reader_.ReadBool();
      if (reserved) {
        DVLOG(1) << "reserved bit set";
        return false;
      }
    } else {
      DVLOG(1) << "4:4:4 color not supported in profile 0 or 2";
      return false;
    }
  }

  return true;
}

void Vp9Parser::ReadFrameSize(Vp9FrameHeader* fhdr) {
  fhdr->width = reader_.ReadLiteral(16) + 1;
  fhdr->height = reader_.ReadLiteral(16) + 1;
}

bool Vp9Parser::ReadFrameSizeFromRefs(Vp9FrameHeader* fhdr) {
  for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
    if (reader_.ReadBool()) {
      fhdr->width = ref_slots_[i].width;
      fhdr->height = ref_slots_[i].height;

      const int kMaxDimension = 1 << 16;
      if (fhdr->width == 0 || fhdr->width > kMaxDimension ||
          fhdr->height == 0 || fhdr->height > kMaxDimension) {
        DVLOG(1) << "The size of reference frame is out of range: "
                 << ref_slots_[i].width << "," << ref_slots_[i].height;
        return false;
      }
      return true;
    }
  }

  fhdr->width = reader_.ReadLiteral(16) + 1;
  fhdr->height = reader_.ReadLiteral(16) + 1;
  return true;
}

void Vp9Parser::ReadDisplayFrameSize(Vp9FrameHeader* fhdr) {
  if (reader_.ReadBool()) {
    fhdr->display_width = reader_.ReadLiteral(16) + 1;
    fhdr->display_height = reader_.ReadLiteral(16) + 1;
  } else {
    fhdr->display_width = fhdr->width;
    fhdr->display_height = fhdr->height;
  }
}

Vp9InterpFilter Vp9Parser::ReadInterpFilter() {
  if (reader_.ReadBool())
    return Vp9InterpFilter::INTERP_FILTER_SELECT;

  // The mapping table for next two bits.
  const Vp9InterpFilter table[] = {
      Vp9InterpFilter::EIGHTTAP_SMOOTH, Vp9InterpFilter::EIGHTTAP,
      Vp9InterpFilter::EIGHTTAP_SHARP, Vp9InterpFilter::BILINEAR,
  };
  return table[reader_.ReadLiteral(2)];
}

void Vp9Parser::ReadLoopFilter(Vp9LoopFilter* loop_filter) {
  loop_filter->filter_level = reader_.ReadLiteral(6);
  loop_filter->sharpness_level = reader_.ReadLiteral(3);

  loop_filter->mode_ref_delta_enabled = reader_.ReadBool();
  if (loop_filter->mode_ref_delta_enabled) {
    loop_filter->mode_ref_delta_update = reader_.ReadBool();
    if (loop_filter->mode_ref_delta_update) {
      for (size_t i = 0; i < Vp9LoopFilter::kNumRefDeltas; i++) {
        loop_filter->update_ref_deltas[i] = reader_.ReadBool();
        if (loop_filter->update_ref_deltas[i])
          loop_filter->ref_deltas[i] = reader_.ReadSignedLiteral(6);
      }

      for (size_t i = 0; i < Vp9LoopFilter::kNumModeDeltas; i++) {
        loop_filter->update_mode_deltas[i] = reader_.ReadBool();
        if (loop_filter->update_mode_deltas[i])
          loop_filter->mode_deltas[i] = reader_.ReadLiteral(6);
      }
    }
  }
}

void Vp9Parser::ReadQuantization(Vp9QuantizationParams* quants) {
  quants->base_qindex = reader_.ReadLiteral(8);

  if (reader_.ReadBool())
    quants->y_dc_delta = reader_.ReadSignedLiteral(4);

  if (reader_.ReadBool())
    quants->uv_ac_delta = reader_.ReadSignedLiteral(4);

  if (reader_.ReadBool())
    quants->uv_dc_delta = reader_.ReadSignedLiteral(4);
}

void Vp9Parser::ReadSegmentationMap(Vp9Segmentation* segment) {
  for (size_t i = 0; i < Vp9Segmentation::kNumTreeProbs; i++) {
    segment->tree_probs[i] =
        reader_.ReadBool() ? reader_.ReadLiteral(8) : kVp9MaxProb;
  }

  for (size_t i = 0; i < Vp9Segmentation::kNumPredictionProbs; i++)
    segment->pred_probs[i] = kVp9MaxProb;

  segment->temporal_update = reader_.ReadBool();
  if (segment->temporal_update) {
    for (size_t i = 0; i < Vp9Segmentation::kNumPredictionProbs; i++) {
      if (reader_.ReadBool())
        segment->pred_probs[i] = reader_.ReadLiteral(8);
    }
  }
}

void Vp9Parser::ReadSegmentationData(Vp9Segmentation* segment) {
  segment->abs_delta = reader_.ReadBool();

  const int kFeatureDataBits[] = {7, 6, 2, 0};
  const bool kFeatureDataSigned[] = {true, true, false, false};

  for (size_t i = 0; i < Vp9Segmentation::kNumSegments; i++) {
    for (size_t j = 0; j < Vp9Segmentation::kNumFeatures; j++) {
      int8_t data = 0;
      segment->feature_enabled[i][j] = reader_.ReadBool();
      if (segment->feature_enabled[i][j]) {
        data = reader_.ReadLiteral(kFeatureDataBits[j]);
        if (kFeatureDataSigned[j])
          if (reader_.ReadBool())
            data = -data;
      }
      segment->feature_data[i][j] = data;
    }
  }
}

void Vp9Parser::ReadSegmentation(Vp9Segmentation* segment) {
  segment->enabled = reader_.ReadBool();

  if (!segment->enabled) {
    return;
  }

  segment->update_map = reader_.ReadBool();
  if (segment->update_map)
    ReadSegmentationMap(segment);

  segment->update_data = reader_.ReadBool();
  if (segment->update_data)
    ReadSegmentationData(segment);
}

void Vp9Parser::ReadTiles(Vp9FrameHeader* fhdr) {
  int sb64_cols = (fhdr->width + 63) / 64;

  int min_log2_tile_cols = GetMinLog2TileCols(sb64_cols);
  int max_log2_tile_cols = GetMaxLog2TileCols(sb64_cols);

  int max_ones = max_log2_tile_cols - min_log2_tile_cols;
  fhdr->log2_tile_cols = min_log2_tile_cols;
  while (max_ones-- && reader_.ReadBool())
    fhdr->log2_tile_cols++;

  if (reader_.ReadBool())
    fhdr->log2_tile_rows = reader_.ReadLiteral(2) - 1;
}

bool Vp9Parser::ParseUncompressedHeader(Vp9FrameHeader* fhdr) {
  reader_.Initialize(stream_, size_);

  // frame marker
  if (reader_.ReadLiteral(2) != 0x2)
    return false;

  fhdr->profile = ReadProfile();
  if (fhdr->profile >= kVp9MaxProfile) {
    DVLOG(1) << "Unsupported bitstream profile";
    return false;
  }

  fhdr->show_existing_frame = reader_.ReadBool();
  if (fhdr->show_existing_frame) {
    fhdr->frame_to_show = reader_.ReadLiteral(3);
    fhdr->show_frame = true;

    if (!reader_.IsValid()) {
      DVLOG(1) << "parser reads beyond the end of buffer";
      return false;
    }
    fhdr->uncompressed_header_size = reader_.GetBytesRead();
    return true;
  }

  fhdr->frame_type = static_cast<Vp9FrameHeader::FrameType>(reader_.ReadBool());
  fhdr->show_frame = reader_.ReadBool();
  fhdr->error_resilient_mode = reader_.ReadBool();

  if (fhdr->IsKeyframe()) {
    if (!VerifySyncCode())
      return false;

    if (!ReadBitDepthColorSpaceSampling(fhdr))
      return false;

    for (size_t i = 0; i < kVp9NumRefFrames; i++)
      fhdr->refresh_flag[i] = true;

    ReadFrameSize(fhdr);
    ReadDisplayFrameSize(fhdr);
  } else {
    if (!fhdr->show_frame)
      fhdr->intra_only = reader_.ReadBool();

    if (!fhdr->error_resilient_mode)
      fhdr->reset_context = reader_.ReadLiteral(2);

    if (fhdr->intra_only) {
      if (!VerifySyncCode())
        return false;

      if (fhdr->profile > 0) {
        if (!ReadBitDepthColorSpaceSampling(fhdr))
          return false;
      } else {
        fhdr->bit_depth = 8;
        fhdr->color_space = Vp9ColorSpace::BT_601;
        fhdr->subsampling_x = fhdr->subsampling_y = 1;
      }

      for (size_t i = 0; i < kVp9NumRefFrames; i++)
        fhdr->refresh_flag[i] = reader_.ReadBool();
      ReadFrameSize(fhdr);
      ReadDisplayFrameSize(fhdr);
    } else {
      for (size_t i = 0; i < kVp9NumRefFrames; i++)
        fhdr->refresh_flag[i] = reader_.ReadBool();

      for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
        fhdr->frame_refs[i] = reader_.ReadLiteral(kVp9NumRefFramesLog2);
        fhdr->ref_sign_biases[i] = reader_.ReadBool();
      }

      if (!ReadFrameSizeFromRefs(fhdr))
        return false;
      ReadDisplayFrameSize(fhdr);

      fhdr->allow_high_precision_mv = reader_.ReadBool();
      fhdr->interp_filter = ReadInterpFilter();
    }
  }

  if (fhdr->error_resilient_mode) {
    fhdr->frame_parallel_decoding_mode = true;
  } else {
    fhdr->refresh_frame_context = reader_.ReadBool();
    fhdr->frame_parallel_decoding_mode = reader_.ReadBool();
  }

  fhdr->frame_context_idx = reader_.ReadLiteral(2);

  ReadLoopFilter(&fhdr->loop_filter);
  ReadQuantization(&fhdr->quant_params);
  ReadSegmentation(&fhdr->segment);

  ReadTiles(fhdr);

  fhdr->first_partition_size = reader_.ReadLiteral(16);
  if (fhdr->first_partition_size == 0) {
    DVLOG(1) << "invalid header size";
    return false;
  }

  if (!reader_.IsValid()) {
    DVLOG(1) << "parser reads beyond the end of buffer";
    return false;
  }
  fhdr->uncompressed_header_size = reader_.GetBytesRead();

  return true;
}

void Vp9Parser::UpdateSlots(const Vp9FrameHeader* fhdr) {
  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    if (fhdr->refresh_flag[i]) {
      ref_slots_[i].width = fhdr->width;
      ref_slots_[i].height = fhdr->height;
    }
  }
}

bool Vp9Parser::ParseFrame(const uint8_t* stream,
                           size_t frame_size,
                           Vp9FrameHeader* fhdr) {
  DCHECK(stream);
  stream_ = stream;
  size_ = frame_size;
  memset(fhdr, 0, sizeof(*fhdr));

  if (!ParseUncompressedHeader(fhdr))
    return false;

  UpdateSlots(fhdr);

  return true;
}

}  // namespace media
