// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp2t/es_parser_h264.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_frame.h"
#include "media/mp2t/mp2t_common.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

static const int kExtendedSar = 255;

// ISO 14496 part 10
// VUI parameters: Table E-1 "Meaning of sample aspect ratio indicator"
static const int kTableSarWidth[14] = {
  0, 1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160
};

static const int kTableSarHeight[14] = {
  0, 1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99
};

// Remove the start code emulation prevention ( 0x000003 )
// and return the size of the converted buffer.
// Note: Size of |buf_rbsp| should be at least |size| to accomodate
// the worst case.
static int ConvertToRbsp(const uint8* buf, int size, uint8* buf_rbsp) {
  int rbsp_size = 0;
  int zero_count = 0;
  for (int k = 0; k < size; k++) {
    if (buf[k] == 0x3 && zero_count >= 2) {
      zero_count = 0;
      continue;
    }
    if (buf[k] == 0)
      zero_count++;
    else
      zero_count = 0;
    buf_rbsp[rbsp_size++] = buf[k];
  }
  return rbsp_size;
}

namespace media {
namespace mp2t {

// ISO 14496 - Part 10: Table 7-1 "NAL unit type codes"
enum NalUnitType {
  kNalUnitTypeNonIdrSlice = 1,
  kNalUnitTypeIdrSlice = 5,
  kNalUnitTypeSPS = 7,
  kNalUnitTypePPS = 8,
  kNalUnitTypeAUD = 9,
};

class BitReaderH264 : public BitReader {
 public:
  BitReaderH264(const uint8* data, off_t size)
    : BitReader(data, size) { }

  // Read an unsigned exp-golomb value.
  // Return true if successful.
  bool ReadBitsExpGolomb(uint32* exp_golomb_value);
};

bool BitReaderH264::ReadBitsExpGolomb(uint32* exp_golomb_value) {
  // Get the number of leading zeros.
  int zero_count = 0;
  while (true) {
    int one_bit;
    RCHECK(ReadBits(1, &one_bit));
    if (one_bit != 0)
      break;
    zero_count++;
  }

  // If zero_count is greater than 31, the calculated value will overflow.
  if (zero_count > 31) {
    SkipBits(zero_count);
    return false;
  }

  // Read the actual value.
  uint32 base = (1 << zero_count) - 1;
  uint32 offset;
  RCHECK(ReadBits(zero_count, &offset));
  *exp_golomb_value = base + offset;

  return true;
}

EsParserH264::EsParserH264(
    const NewVideoConfigCB& new_video_config_cb,
    const EmitBufferCB& emit_buffer_cb)
  : new_video_config_cb_(new_video_config_cb),
    emit_buffer_cb_(emit_buffer_cb),
    es_pos_(0),
    current_nal_pos_(-1),
    current_access_unit_pos_(-1),
    is_key_frame_(false) {
}

EsParserH264::~EsParserH264() {
}

bool EsParserH264::Parse(const uint8* buf, int size,
                         base::TimeDelta pts,
                         base::TimeDelta dts) {
  // Note: Parse is invoked each time a PES packet has been reassembled.
  // Unfortunately, a PES packet does not necessarily map
  // to an h264 access unit, although the HLS recommendation is to use one PES
  // for each access unit (but this is just a recommendation and some streams
  // do not comply with this recommendation).

  // Link position |raw_es_size| in the ES stream with a timing descriptor.
  // HLS recommendation: "In AVC video, you should have both a DTS and a
  // PTS in each PES header".
  if (dts == kNoTimestamp() && pts == kNoTimestamp()) {
    DVLOG(1) << "A timestamp must be provided for each reassembled PES";
    return false;
  }
  TimingDesc timing_desc;
  timing_desc.pts = pts;
  timing_desc.dts = (dts != kNoTimestamp()) ? dts : pts;

  int raw_es_size;
  const uint8* raw_es;
  es_byte_queue_.Peek(&raw_es, &raw_es_size);
  timing_desc_list_.push_back(
      std::pair<int, TimingDesc>(raw_es_size, timing_desc));

  // Add the incoming bytes to the ES queue.
  es_byte_queue_.Push(buf, size);

  // Add NALs from the incoming buffer.
  if (!ParseInternal())
    return false;

  // Discard emitted frames
  // or every byte that was parsed so far if there is no current frame.
  int skip_count =
      (current_access_unit_pos_ >= 0) ? current_access_unit_pos_ : es_pos_;
  DiscardEs(skip_count);

  return true;
}

void EsParserH264::Flush() {
  if (current_access_unit_pos_ < 0)
    return;

  // Force emitting the last access unit.
  int next_aud_pos;
  const uint8* raw_es;
  es_byte_queue_.Peek(&raw_es, &next_aud_pos);
  EmitFrameIfNeeded(next_aud_pos);
  current_nal_pos_ = -1;
  StartFrame(-1);

  // Discard the emitted frame.
  DiscardEs(next_aud_pos);
}

void EsParserH264::Reset() {
  DVLOG(1) << "EsParserH264::Reset";
  es_byte_queue_.Reset();
  timing_desc_list_.clear();
  es_pos_ = 0;
  current_nal_pos_ = -1;
  StartFrame(-1);
  last_video_decoder_config_ = VideoDecoderConfig();
}

bool EsParserH264::ParseInternal() {
  int raw_es_size;
  const uint8* raw_es;
  es_byte_queue_.Peek(&raw_es, &raw_es_size);

  DCHECK_GE(es_pos_, 0);
  DCHECK_LT(es_pos_, raw_es_size);

  // Resume h264 es parsing where it was left.
  for ( ; es_pos_ < raw_es_size - 4; es_pos_++) {
    // Make sure the syncword is either 00 00 00 01 or 00 00 01
    if (raw_es[es_pos_ + 0] != 0 || raw_es[es_pos_ + 1] != 0)
      continue;
    int syncword_length = 0;
    if (raw_es[es_pos_ + 2] == 0 && raw_es[es_pos_ + 3] == 1)
      syncword_length = 4;
    else if (raw_es[es_pos_ + 2] == 1)
      syncword_length = 3;
    else
      continue;

    // Parse the current NAL (and the new NAL then becomes the current one).
    if (current_nal_pos_ >= 0) {
      int nal_size = es_pos_ - current_nal_pos_;
      DCHECK_GT(nal_size, 0);
      RCHECK(NalParser(&raw_es[current_nal_pos_], nal_size));
    }
    current_nal_pos_ = es_pos_ + syncword_length;

    // Retrieve the NAL type.
    int nal_header = raw_es[current_nal_pos_];
    int forbidden_zero_bit = (nal_header >> 7) & 0x1;
    RCHECK(forbidden_zero_bit == 0);
    NalUnitType nal_unit_type = static_cast<NalUnitType>(nal_header & 0x1f);
    DVLOG(LOG_LEVEL_ES) << "nal: offset=" << es_pos_
                        << " type=" << nal_unit_type;

    // Emit a frame if needed.
    if (nal_unit_type == kNalUnitTypeAUD)
      EmitFrameIfNeeded(es_pos_);

    // Skip the syncword.
    es_pos_ += syncword_length;
  }

  return true;
}

void EsParserH264::EmitFrameIfNeeded(int next_aud_pos) {
  // There is no current frame: start a new frame.
  if (current_access_unit_pos_ < 0) {
    StartFrame(next_aud_pos);
    return;
  }

  // Get the access unit timing info.
  TimingDesc current_timing_desc;
  while (!timing_desc_list_.empty() &&
         timing_desc_list_.front().first <= current_access_unit_pos_) {
    current_timing_desc = timing_desc_list_.front().second;
    timing_desc_list_.pop_front();
  }

  // Emit a frame.
  int raw_es_size;
  const uint8* raw_es;
  es_byte_queue_.Peek(&raw_es, &raw_es_size);
  int access_unit_size = next_aud_pos - current_access_unit_pos_;
  scoped_refptr<StreamParserBuffer> stream_parser_buffer =
      StreamParserBuffer::CopyFrom(
          &raw_es[current_access_unit_pos_],
          access_unit_size,
          is_key_frame_);
  stream_parser_buffer->SetDecodeTimestamp(current_timing_desc.dts);
  stream_parser_buffer->set_timestamp(current_timing_desc.pts);
  emit_buffer_cb_.Run(stream_parser_buffer);

  // Set the current frame position to the next AUD position.
  StartFrame(next_aud_pos);
}

void EsParserH264::StartFrame(int aud_pos) {
  // Two cases:
  // - if aud_pos < 0, clear the current frame and set |is_key_frame| to a
  // default value (false).
  // - if aud_pos >= 0, start a new frame and set |is_key_frame| to true
  // |is_key_frame_| will be updated while parsing the NALs of that frame.
  // If any NAL is a non IDR NAL, it will be set to false.
  current_access_unit_pos_ = aud_pos;
  is_key_frame_ = (aud_pos >= 0);
}

void EsParserH264::DiscardEs(int nbytes) {
  DCHECK_GE(nbytes, 0);
  if (nbytes == 0)
    return;

  // Update the position of
  // - the parser,
  // - the current NAL,
  // - the current access unit.
  es_pos_ -= nbytes;
  if (es_pos_ < 0)
    es_pos_ = 0;

  if (current_nal_pos_ >= 0) {
    DCHECK_GE(current_nal_pos_, nbytes);
    current_nal_pos_ -= nbytes;
  }
  if (current_access_unit_pos_ >= 0) {
    DCHECK_GE(current_access_unit_pos_, nbytes);
    current_access_unit_pos_ -= nbytes;
  }

  // Update the timing information accordingly.
  std::list<std::pair<int, TimingDesc> >::iterator timing_it
      = timing_desc_list_.begin();
  for (; timing_it != timing_desc_list_.end(); ++timing_it)
    timing_it->first -= nbytes;

  // Discard |nbytes| of ES.
  es_byte_queue_.Pop(nbytes);
}

bool EsParserH264::NalParser(const uint8* buf, int size) {
  // Get the NAL header.
  if (size < 1) {
    DVLOG(1) << "NalParser: incomplete NAL";
    return false;
  }
  int nal_header = buf[0];
  buf += 1;
  size -= 1;

  int forbidden_zero_bit = (nal_header >> 7) & 0x1;
  if (forbidden_zero_bit != 0)
    return false;
  int nal_ref_idc = (nal_header >> 5) & 0x3;
  int nal_unit_type = nal_header & 0x1f;

  // Process the NAL content.
  switch (nal_unit_type) {
    case kNalUnitTypeSPS:
      DVLOG(LOG_LEVEL_ES) << "NAL: SPS";
      // |nal_ref_idc| should not be 0 for a SPS.
      if (nal_ref_idc == 0)
        return false;
      return ProcessSPS(buf, size);
    case kNalUnitTypeIdrSlice:
      DVLOG(LOG_LEVEL_ES) << "NAL: IDR slice";
      return true;
    case kNalUnitTypeNonIdrSlice:
      DVLOG(LOG_LEVEL_ES) << "NAL: Non IDR slice";
      is_key_frame_ = false;
      return true;
    case kNalUnitTypePPS:
      DVLOG(LOG_LEVEL_ES) << "NAL: PPS";
      return true;
    case  kNalUnitTypeAUD:
      DVLOG(LOG_LEVEL_ES) << "NAL: AUD";
      return true;
    default:
      DVLOG(LOG_LEVEL_ES) << "NAL: " << nal_unit_type;
      return true;
  }

  NOTREACHED();
  return false;
}

bool EsParserH264::ProcessSPS(const uint8* buf, int size) {
  if (size <= 0)
    return false;

  // Removes start code emulation prevention.
  // TODO(damienv): refactoring in media/base
  // so as to have a unique H264 bit reader in Chrome.
  scoped_ptr<uint8[]> buf_rbsp(new uint8[size]);
  int rbsp_size = ConvertToRbsp(buf, size, buf_rbsp.get());

  BitReaderH264 bit_reader(buf_rbsp.get(), rbsp_size);

  int profile_idc;
  int constraint_setX_flag;
  int level_idc;
  uint32 seq_parameter_set_id;
  uint32 log2_max_frame_num_minus4;
  uint32 pic_order_cnt_type;
  RCHECK(bit_reader.ReadBits(8, &profile_idc));
  RCHECK(bit_reader.ReadBits(8, &constraint_setX_flag));
  RCHECK(bit_reader.ReadBits(8, &level_idc));
  RCHECK(bit_reader.ReadBitsExpGolomb(&seq_parameter_set_id));
  RCHECK(bit_reader.ReadBitsExpGolomb(&log2_max_frame_num_minus4));
  RCHECK(bit_reader.ReadBitsExpGolomb(&pic_order_cnt_type));

  // |pic_order_cnt_type| shall be in the range of 0 to 2.
  RCHECK(pic_order_cnt_type <= 2);
  if (pic_order_cnt_type == 0) {
    uint32 log2_max_pic_order_cnt_lsb_minus4;
    RCHECK(bit_reader.ReadBitsExpGolomb(&log2_max_pic_order_cnt_lsb_minus4));
  } else if (pic_order_cnt_type == 1) {
    // Note: |offset_for_non_ref_pic| and |offset_for_top_to_bottom_field|
    // corresponds to their codenum not to their actual value.
    int delta_pic_order_always_zero_flag;
    uint32 offset_for_non_ref_pic;
    uint32 offset_for_top_to_bottom_field;
    uint32 num_ref_frames_in_pic_order_cnt_cycle;
    RCHECK(bit_reader.ReadBits(1, &delta_pic_order_always_zero_flag));
    RCHECK(bit_reader.ReadBitsExpGolomb(&offset_for_non_ref_pic));
    RCHECK(bit_reader.ReadBitsExpGolomb(&offset_for_top_to_bottom_field));
    RCHECK(
        bit_reader.ReadBitsExpGolomb(&num_ref_frames_in_pic_order_cnt_cycle));
    for (uint32 i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
      uint32 offset_for_ref_frame_codenum;
      RCHECK(bit_reader.ReadBitsExpGolomb(&offset_for_ref_frame_codenum));
    }
  }

  uint32 num_ref_frames;
  int gaps_in_frame_num_value_allowed_flag;
  uint32 pic_width_in_mbs_minus1;
  uint32 pic_height_in_map_units_minus1;
  RCHECK(bit_reader.ReadBitsExpGolomb(&num_ref_frames));
  RCHECK(bit_reader.ReadBits(1, &gaps_in_frame_num_value_allowed_flag));
  RCHECK(bit_reader.ReadBitsExpGolomb(&pic_width_in_mbs_minus1));
  RCHECK(bit_reader.ReadBitsExpGolomb(&pic_height_in_map_units_minus1));

  int frame_mbs_only_flag;
  RCHECK(bit_reader.ReadBits(1, &frame_mbs_only_flag));
  if (!frame_mbs_only_flag) {
    int mb_adaptive_frame_field_flag;
    RCHECK(bit_reader.ReadBits(1, &mb_adaptive_frame_field_flag));
  }

  int direct_8x8_inference_flag;
  RCHECK(bit_reader.ReadBits(1, &direct_8x8_inference_flag));

  int frame_cropping_flag;
  uint32 frame_crop_left_offset = 0;
  uint32 frame_crop_right_offset = 0;
  uint32 frame_crop_top_offset = 0;
  uint32 frame_crop_bottom_offset = 0;
  RCHECK(bit_reader.ReadBits(1, &frame_cropping_flag));
  if (frame_cropping_flag) {
    RCHECK(bit_reader.ReadBitsExpGolomb(&frame_crop_left_offset));
    RCHECK(bit_reader.ReadBitsExpGolomb(&frame_crop_right_offset));
    RCHECK(bit_reader.ReadBitsExpGolomb(&frame_crop_top_offset));
    RCHECK(bit_reader.ReadBitsExpGolomb(&frame_crop_bottom_offset));
  }

  int vui_parameters_present_flag;
  RCHECK(bit_reader.ReadBits(1, &vui_parameters_present_flag));
  int sar_width = 1;
  int sar_height = 1;
  if (vui_parameters_present_flag) {
    // Read only the aspect ratio information from the VUI section.
    // TODO(damienv): check whether other VUI info are useful.
    int aspect_ratio_info_present_flag;
    RCHECK(bit_reader.ReadBits(1, &aspect_ratio_info_present_flag));
    if (aspect_ratio_info_present_flag) {
      int aspect_ratio_idc;
      RCHECK(bit_reader.ReadBits(8, &aspect_ratio_idc));
      if (aspect_ratio_idc == kExtendedSar) {
        RCHECK(bit_reader.ReadBits(16, &sar_width));
        RCHECK(bit_reader.ReadBits(16, &sar_height));
      } else if (aspect_ratio_idc < 14) {
        sar_width = kTableSarWidth[aspect_ratio_idc];
        sar_height = kTableSarHeight[aspect_ratio_idc];
      }
    }
  }

  if (sar_width == 0 || sar_height == 0) {
    DVLOG(1) << "Unspecified SAR not supported";
    return false;
  }

  // TODO(damienv): a MAP unit can be either 16 or 32 pixels.
  // although it's 16 pixels for progressive non MBAFF frames.
  gfx::Size coded_size((pic_width_in_mbs_minus1 + 1) * 16,
                       (pic_height_in_map_units_minus1 + 1) * 16);
  gfx::Rect visible_rect(
      frame_crop_left_offset,
      frame_crop_top_offset,
      (coded_size.width() - frame_crop_right_offset) - frame_crop_left_offset,
      (coded_size.height() - frame_crop_bottom_offset) - frame_crop_top_offset);
  if (visible_rect.width() <= 0 || visible_rect.height() <= 0)
    return false;
  gfx::Size natural_size((visible_rect.width() * sar_width) / sar_height,
                         visible_rect.height());
  if (natural_size.width() == 0)
    return false;

  // TODO(damienv):
  // Assuming the SPS is used right away by the PPS
  // and the slice headers is a strong assumption.
  // In theory, we should process the SPS and PPS
  // and only when one of the slice header is switching
  // the PPS id, the video decoder config should be changed.
  VideoDecoderConfig video_decoder_config(
      kCodecH264,
      VIDEO_CODEC_PROFILE_UNKNOWN,    // TODO(damienv)
      VideoFrame::YV12,
      coded_size,
      visible_rect,
      natural_size,
      NULL, 0,
      false);

  if (!video_decoder_config.Matches(last_video_decoder_config_)) {
    DVLOG(1) << "Profile IDC: " << profile_idc;
    DVLOG(1) << "Level IDC: " << level_idc;
    DVLOG(1) << "Pic width: " << (pic_width_in_mbs_minus1 + 1) * 16;
    DVLOG(1) << "Pic height: " << (pic_height_in_map_units_minus1 + 1) * 16;
    DVLOG(1) << "log2_max_frame_num_minus4: " << log2_max_frame_num_minus4;
    DVLOG(1) << "SAR: width=" << sar_width << " height=" << sar_height;
    last_video_decoder_config_ = video_decoder_config;
    new_video_config_cb_.Run(video_decoder_config);
  }

  return true;
}

}  // namespace mp2t
}  // namespace media

