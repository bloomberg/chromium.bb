// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/h264_parser.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"

namespace content {

bool H264SliceHeader::IsPSlice() const {
  return (slice_type % 5 == kPSlice);
}

bool H264SliceHeader::IsBSlice() const {
  return (slice_type % 5 == kBSlice);
}

bool H264SliceHeader::IsISlice() const {
  return (slice_type % 5 == kISlice);
}

bool H264SliceHeader::IsSPSlice() const {
  return (slice_type % 5 == kSPSlice);
}

bool H264SliceHeader::IsSISlice() const {
  return (slice_type % 5 == kSISlice);
}

H264NALU::H264NALU() {
  memset(this, 0, sizeof(*this));
}

H264SPS::H264SPS() {
  memset(this, 0, sizeof(*this));
}

H264PPS::H264PPS() {
  memset(this, 0, sizeof(*this));
}

H264SliceHeader::H264SliceHeader() {
  memset(this, 0, sizeof(*this));
}

H264SEIMessage::H264SEIMessage() {
  memset(this, 0, sizeof(*this));
}

H264Parser::H264BitReader::H264BitReader()
    : data_(NULL),
      bytes_left_(0),
      curr_byte_(0),
      num_remaining_bits_in_curr_byte_(0) {
}

H264Parser::H264BitReader::~H264BitReader() {}

bool H264Parser::H264BitReader::Initialize(const uint8* data, off_t size) {
  DCHECK(data);

  if (size < 1)
    return false;

  data_ = data;
  bytes_left_ = size;
  num_remaining_bits_in_curr_byte_ = 0;
  // Initially set to 0xffff to accept all initial two-byte sequences.
  prev_two_bytes_ = 0xffff;

  return true;
}

bool H264Parser::H264BitReader::UpdateCurrByte() {
  if (bytes_left_ < 1)
    return false;

  // Emulation prevention three-byte detection.
  // If a sequence of 0x000003 is found, skip (ignore) the last byte (0x03).
  if (*data_ == 0x03 && (prev_two_bytes_ & 0xffff) == 0) {
    // Detected 0x000003, skip last byte.
    ++data_;
    --bytes_left_;
    // Need another full three bytes before we can detect the sequence again.
    prev_two_bytes_ = 0xffff;

    if (bytes_left_ < 1)
      return false;
  }

  // Load a new byte and advance pointers.
  curr_byte_ = *data_++ & 0xff;
  --bytes_left_;
  num_remaining_bits_in_curr_byte_ = 8;

  prev_two_bytes_ = (prev_two_bytes_ << 8) | curr_byte_;

  return true;
}

// Read |num_bits| (1 to 31 inclusive) from the stream and return them
// in |out|, with first bit in the stream as MSB in |out| at position
// (|num_bits| - 1).
bool H264Parser::H264BitReader::ReadBits(int num_bits, int *out) {
  int bits_left = num_bits;
  *out = 0;
  DCHECK(num_bits <= 31);

  while (num_remaining_bits_in_curr_byte_ < bits_left) {
    // Take all that's left in current byte, shift to make space for the rest.
    *out = (curr_byte_ << (bits_left - num_remaining_bits_in_curr_byte_));
    bits_left -= num_remaining_bits_in_curr_byte_;

    if (!UpdateCurrByte())
      return false;
  }

  *out |= (curr_byte_ >> (num_remaining_bits_in_curr_byte_ - bits_left));
  *out &= ((1 << num_bits) - 1);
  num_remaining_bits_in_curr_byte_ -= bits_left;

  return true;
}

off_t H264Parser::H264BitReader::NumBitsLeft() {
  return (num_remaining_bits_in_curr_byte_ + bytes_left_ * 8);
}

bool H264Parser::H264BitReader::HasMoreRBSPData() {
  // Make sure we have more bits, if we are at 0 bits in current byte
  // and updating current byte fails, we don't have more data anyway.
  if (num_remaining_bits_in_curr_byte_ == 0 && !UpdateCurrByte())
      return false;

  // On last byte?
  if (bytes_left_)
    return true;

  // Last byte, look for stop bit;
  // We have more RBSP data if the last non-zero bit we find is not the
  // first available bit.
  return curr_byte_ & ((1 << (num_remaining_bits_in_curr_byte_ - 1)) - 1);
}

#define READ_BITS_OR_RETURN(num_bits, out)                                    \
do {                                                                          \
  int _out;                                                                   \
  if (!br_.ReadBits(num_bits, &_out)) {                                       \
    DVLOG(1) << "Error in stream: unexpected EOS while trying to read " #out; \
    return kInvalidStream;                                                    \
  }                                                                           \
  *out = _out;                                                                \
} while (0)

#define READ_UE_OR_RETURN(out)                                               \
do {                                                                         \
  if (ReadUE(out) != kOk) {                                                  \
    DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
    return kInvalidStream;                                                   \
  }                                                                          \
} while (0)

#define READ_SE_OR_RETURN(out)                                               \
do {                                                                         \
  if (ReadSE(out) != kOk) {                                                  \
    DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
    return kInvalidStream;                                                   \
  }                                                                          \
} while (0)

#define IN_RANGE_OR_RETURN(val, min, max)                                 \
do {                                                                      \
  if ((val) < (min) || (val) > (max)) {                                   \
    DVLOG(1) << "Error in stream: invalid value, expected " #val " to be" \
             << " in range [" << (min) << ":" << (max) << "]"             \
             << " found " << (val) << " instead";                         \
    return kInvalidStream;                                                \
  }                                                                       \
} while (0)

#define TRUE_OR_RETURN(a)                                          \
do {                                                               \
  if (!(a)) {                                                      \
    DVLOG(1) << "Error in stream: invalid value, expected " << #a; \
    return kInvalidStream;                                         \
  }                                                                \
} while (0)

H264Parser::H264Parser() {
  Reset();
}

H264Parser::~H264Parser() {
  STLDeleteValues(&active_SPSes_);
  STLDeleteValues(&active_PPSes_);
}

void H264Parser::Reset() {
  stream_ = NULL;
  bytes_left_ = 0;
}

void H264Parser::SetStream(const uint8* stream, off_t stream_size) {
  DCHECK(stream);
  DCHECK(stream_size > 0);

  stream_ = stream;
  bytes_left_ = stream_size;
}

const H264PPS* H264Parser::GetPPS(int pps_id) {
  return active_PPSes_[pps_id];
}

const H264SPS* H264Parser::GetSPS(int sps_id) {
  return active_SPSes_[sps_id];
}

static inline bool IsStartCode(const uint8* data) {
  return data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01;
}

// Find offset from start of data to next NALU start code
// and size of found start code (3 or 4 bytes).
static bool FindStartCode(const uint8* data, off_t data_size,
                          off_t* offset,
                          off_t* start_code_size) {
  off_t bytes_left = data_size;

  while (bytes_left > 3) {
    if (IsStartCode(data)) {
      // Found three-byte start code, set pointer at its beginning.
      *offset = data_size - bytes_left;
      *start_code_size = 3;

      // If there is a zero byte before this start code,
      // then it's actually a four-byte start code, so backtrack one byte.
      if (*offset > 0 && *(data - 1) == 0x00) {
        --(*offset);
        ++(*start_code_size);
      }

      return true;
    }

    ++data;
    --bytes_left;
  }

  // End of data.
  return false;
}

// Find the next NALU in stream, returning its start offset without the start
// code (i.e. at the beginning of NALU data).
// Size will include trailing zero bits, and will be from start offset to
// before the start code of the next NALU (or end of stream).
static bool LocateNALU(const uint8* stream, off_t stream_size,
                       off_t* nalu_start_off, off_t* nalu_size) {
  off_t start_code_size;

  // Find start code of the next NALU.
  if (!FindStartCode(stream, stream_size, nalu_start_off, &start_code_size)) {
    DVLOG(4) << "Could not find start code, end of stream?";
    return false;
  }

  // Discard its start code.
  *nalu_start_off += start_code_size;
  // Move the stream to the beginning of it (skip the start code).
  stream_size -= *nalu_start_off;
  stream += *nalu_start_off;

  // Find the start code of next NALU; if successful, NALU size is the number
  // of bytes from after previous start code to before this one;
  // if next start code is not found, it is still a valid NALU if there
  // are still some bytes left after the first start code.
  // nalu_size is the offset to the next start code
  if (!FindStartCode(stream, stream_size, nalu_size, &start_code_size)) {
    // end of stream (no next NALU), but still valid NALU if any bytes left
    *nalu_size = stream_size;
    if (*nalu_size < 1) {
      DVLOG(3) << "End of stream";
      return false;
    }
  }

  return true;
}

H264Parser::Result H264Parser::ReadUE(int* val) {
  int num_bits = -1;
  int bit;
  int rest;

  // Count the number of contiguous zero bits.
  do {
    READ_BITS_OR_RETURN(1, &bit);
    num_bits++;
  } while (bit == 0);

  if (num_bits > 31)
    return kInvalidStream;

  // Calculate exp-Golomb code value of size num_bits.
  *val = (1 << num_bits) - 1;

  if (num_bits > 0) {
    READ_BITS_OR_RETURN(num_bits, &rest);
    *val += rest;
  }

  return kOk;
}

H264Parser::Result H264Parser::ReadSE(int* val) {
  int ue;
  Result res;

  // See Chapter 9 in the spec.
  res = ReadUE(&ue);
  if (res != kOk)
    return res;

  if (ue % 2 == 0)
    *val = -(ue / 2);
  else
    *val = ue / 2 + 1;

  return kOk;
}

H264Parser::Result H264Parser::AdvanceToNextNALU(H264NALU *nalu) {
  int data;
  off_t off_to_nalu_start;

  if (!LocateNALU(stream_, bytes_left_, &off_to_nalu_start, &nalu->size)) {
    DVLOG(4) << "Could not find next NALU, bytes left in stream: "
             << bytes_left_;
    return kEOStream;
  }

  nalu->data = stream_ + off_to_nalu_start;

  // Initialize bit reader at the start of found NALU.
  if (!br_.Initialize(nalu->data, nalu->size))
    return kEOStream;

  DVLOG(4) << "Looking for NALU, Stream bytes left: " << bytes_left_
           << " off to next nalu: " << off_to_nalu_start;

  // Move parser state to after this NALU, so next time AdvanceToNextNALU
  // is called, we will effectively be skipping it;
  // other parsing functions will use the position saved
  // in bit reader for parsing, so we don't have to remember it here.
  stream_ += off_to_nalu_start + nalu->size;
  bytes_left_ -= off_to_nalu_start + nalu->size;

  // Read NALU header, skip the forbidden_zero_bit, but check for it.
  READ_BITS_OR_RETURN(1, &data);
  TRUE_OR_RETURN(data == 0);

  READ_BITS_OR_RETURN(2, &nalu->nal_ref_idc);
  READ_BITS_OR_RETURN(5, &nalu->nal_unit_type);

  DVLOG(4) << "NALU type: " << (int)nalu->nal_unit_type
           << " at: " << (void*)nalu->data << " size: " << nalu->size
           << " ref: " << (int)nalu->nal_ref_idc;

  return kOk;
}

// Default scaling lists (per spec).
static const int kDefault4x4Intra[kH264ScalingList4x4Length] = {
  6, 13, 13, 20, 20, 20, 28, 28, 28, 28, 32, 32, 32, 37, 37, 42, };

static const int kDefault4x4Inter[kH264ScalingList4x4Length] = {
  10, 14, 14, 20, 20, 20, 24, 24, 24, 24, 27, 27, 27, 30, 30, 34, };

static const int kDefault8x8Intra[kH264ScalingList8x8Length] = {
  6, 10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 23,
  23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
  27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
  31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42, };

static const int kDefault8x8Inter[kH264ScalingList8x8Length] = {
  9, 13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
  21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
  24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
  27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35, };

static inline void DefaultScalingList4x4(
    int i,
    int scaling_list4x4[][kH264ScalingList4x4Length]) {
  DCHECK(i < 6);

  if (i < 3)
    memcpy(scaling_list4x4[i], kDefault4x4Intra, sizeof(kDefault4x4Intra));
  else if (i < 6)
    memcpy(scaling_list4x4[i], kDefault4x4Inter, sizeof(kDefault4x4Inter));
}

static inline void DefaultScalingList8x8(
    int i,
    int scaling_list8x8[][kH264ScalingList8x8Length]) {
  DCHECK(i < 6);

  if (i % 2 == 0)
    memcpy(scaling_list8x8[i], kDefault8x8Intra, sizeof(kDefault8x8Intra));
  else
    memcpy(scaling_list8x8[i], kDefault8x8Inter, sizeof(kDefault8x8Inter));
}

static void FallbackScalingList4x4(
    int i,
    const int default_scaling_list_intra[],
    const int default_scaling_list_inter[],
    int scaling_list4x4[][kH264ScalingList4x4Length]) {
  static const int kScalingList4x4ByteSize = sizeof(scaling_list4x4[0][0]) *
      kH264ScalingList4x4Length;

  switch (i) {
    case 0:
      memcpy(scaling_list4x4[i], default_scaling_list_intra,
             kScalingList4x4ByteSize);
      break;

    case 1:
      memcpy(scaling_list4x4[i], scaling_list4x4[0],
             kScalingList4x4ByteSize);
      break;

    case 2:
      memcpy(scaling_list4x4[i], scaling_list4x4[1],
             kScalingList4x4ByteSize);
      break;

    case 3:
      memcpy(scaling_list4x4[i], default_scaling_list_inter,
             kScalingList4x4ByteSize);
      break;

    case 4:
      memcpy(scaling_list4x4[i], scaling_list4x4[3],
             kScalingList4x4ByteSize);
      break;

    case 5:
      memcpy(scaling_list4x4[i], scaling_list4x4[4],
             kScalingList4x4ByteSize);
      break;

    default:
      NOTREACHED();
      break;
  }
}

static void FallbackScalingList8x8(
    int i,
    const int default_scaling_list_intra[],
    const int default_scaling_list_inter[],
    int scaling_list8x8[][kH264ScalingList8x8Length]) {
  static const int kScalingList8x8ByteSize = sizeof(scaling_list8x8[0][0]) *
      kH264ScalingList8x8Length;

  switch (i) {
    case 0:
      memcpy(scaling_list8x8[i], default_scaling_list_intra,
             kScalingList8x8ByteSize);
      break;

    case 1:
      memcpy(scaling_list8x8[i], default_scaling_list_inter,
             kScalingList8x8ByteSize);
      break;

    case 2:
      memcpy(scaling_list8x8[i], scaling_list8x8[0],
             kScalingList8x8ByteSize);
      break;

    case 3:
      memcpy(scaling_list8x8[i], scaling_list8x8[1],
             kScalingList8x8ByteSize);
      break;

    case 4:
      memcpy(scaling_list8x8[i], scaling_list8x8[2],
             kScalingList8x8ByteSize);
      break;

    case 5:
      memcpy(scaling_list8x8[i], scaling_list8x8[3],
             kScalingList8x8ByteSize);
      break;

    default:
      NOTREACHED();
      break;
  }
}

H264Parser::Result H264Parser::ParseScalingList(int size,
                                                int* scaling_list,
                                                bool* use_default) {
  // See chapter 7.3.2.1.1.1.
  int last_scale = 8;
  int next_scale = 8;
  int delta_scale;

  *use_default = false;

  for (int j = 0; j < size; ++j) {
    if (next_scale != 0) {
      READ_SE_OR_RETURN(&delta_scale);
      IN_RANGE_OR_RETURN(delta_scale, -128, 127);
      next_scale = (last_scale + delta_scale + 256) & 0xff;

      if (j == 0 && next_scale == 0) {
        *use_default = true;
        return kOk;
      }
    }

    scaling_list[j] = (next_scale == 0) ? last_scale : next_scale;
    last_scale = scaling_list[j];
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseSPSScalingLists(H264SPS* sps) {
  // See 7.4.2.1.1.
  bool seq_scaling_list_present_flag;
  bool use_default;
  Result res;

  // Parse scaling_list4x4.
  for (int i = 0; i < 6; ++i) {
    READ_BITS_OR_RETURN(1, &seq_scaling_list_present_flag);

    if (seq_scaling_list_present_flag) {
      res = ParseScalingList(sizeof(sps->scaling_list4x4[i]),
                             sps->scaling_list4x4[i], &use_default);
      if (res != kOk)
        return res;

      if (use_default)
        DefaultScalingList4x4(i, sps->scaling_list4x4);

    } else {
        FallbackScalingList4x4(i, kDefault4x4Intra, kDefault4x4Inter,
                               sps->scaling_list4x4);
    }
  }

  // Parse scaling_list8x8.
  for (int i = 0; i < (sps->chroma_format_idc != 3) ? 2 : 6; ++i) {
    READ_BITS_OR_RETURN(1, &seq_scaling_list_present_flag);

    if (seq_scaling_list_present_flag) {
      res = ParseScalingList(sizeof(sps->scaling_list8x8[i]),
                             sps->scaling_list8x8[i], &use_default);
      if (res != kOk)
        return res;

      if (use_default)
        DefaultScalingList8x8(i, sps->scaling_list8x8);

    } else {
        FallbackScalingList8x8(i, kDefault8x8Intra, kDefault8x8Inter,
                              sps->scaling_list8x8);
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParsePPSScalingLists(const H264SPS& sps,
                                                    H264PPS* pps) {
  // See 7.4.2.2.
  bool pic_scaling_list_present_flag;
  bool use_default;
  Result res;

  for (int i = 0; i < 6; ++i) {
    READ_BITS_OR_RETURN(1, &pic_scaling_list_present_flag);

    if (pic_scaling_list_present_flag) {
      res = ParseScalingList(sizeof(pps->scaling_list4x4[i]),
                             pps->scaling_list4x4[i], &use_default);
      if (res != kOk)
        return res;

      if (use_default)
        DefaultScalingList4x4(i, pps->scaling_list4x4);

    } else {
      if (sps.seq_scaling_matrix_present_flag) {
        // Table 7-2 fallback rule A in spec.
        FallbackScalingList4x4(i, kDefault4x4Intra, kDefault4x4Inter,
                               pps->scaling_list4x4);
      } else {
        // Table 7-2 fallback rule B in spec.
        FallbackScalingList4x4(i, sps.scaling_list4x4[0],
                               sps.scaling_list4x4[3], pps->scaling_list4x4);
      }
    }
  }

  if (pps->transform_8x8_mode_flag) {
    for (int i = 0; i < (sps.chroma_format_idc != 3) ? 2 : 6; ++i) {
      READ_BITS_OR_RETURN(1, &pic_scaling_list_present_flag);

      if (pic_scaling_list_present_flag) {
        res = ParseScalingList(sizeof(pps->scaling_list8x8[i]),
                               pps->scaling_list8x8[i], &use_default);
        if (res != kOk)
          return res;

        if (use_default)
          DefaultScalingList8x8(i, pps->scaling_list8x8);

      } else {
        if (sps.seq_scaling_matrix_present_flag) {
          // Table 7-2 fallback rule A in spec.
          FallbackScalingList8x8(i, kDefault8x8Intra,
                                 kDefault8x8Inter, pps->scaling_list8x8);
        } else {
          // Table 7-2 fallback rule B in spec.
          FallbackScalingList8x8(i, sps.scaling_list8x8[0],
                                 sps.scaling_list8x8[1], pps->scaling_list8x8);
        }
      }
    }
  }
  return kOk;
}

static void FillDefaultSeqScalingLists(H264SPS* sps) {
  // Assumes ints in arrays.
  memset(sps->scaling_list4x4, 16, sizeof(sps->scaling_list4x4));
  memset(sps->scaling_list8x8, 16, sizeof(sps->scaling_list8x8));
}

H264Parser::Result H264Parser::ParseSPS(int* sps_id) {
  // See 7.4.2.1.
  int data;
  Result res;

  *sps_id = -1;

  scoped_ptr<H264SPS> sps(new H264SPS());

  READ_BITS_OR_RETURN(8, &sps->profile_idc);
  // Skip constraint_setx_flag and reserved flags.
  READ_BITS_OR_RETURN(8, &data);
  READ_BITS_OR_RETURN(8, &sps->level_idc);
  READ_UE_OR_RETURN(&sps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps->seq_parameter_set_id < 32);

  if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
      sps->profile_idc == 122 || sps->profile_idc == 244 ||
      sps->profile_idc ==  44 || sps->profile_idc ==  83 ||
      sps->profile_idc ==  86 || sps->profile_idc == 118 ||
      sps->profile_idc == 128) {
    READ_UE_OR_RETURN(&sps->chroma_format_idc);
    TRUE_OR_RETURN(sps->chroma_format_idc < 4);

    if (sps->chroma_format_idc == 3)
      READ_BITS_OR_RETURN(1, &sps->separate_colour_plane_flag);

    if (sps->separate_colour_plane_flag)
      sps->chroma_array_type = 0;
    else
      sps->chroma_array_type = sps->chroma_format_idc;

    READ_UE_OR_RETURN(&sps->bit_depth_luma_minus8);
    TRUE_OR_RETURN(sps->bit_depth_luma_minus8 < 7);

    READ_UE_OR_RETURN(&sps->bit_depth_chroma_minus8);
    TRUE_OR_RETURN(sps->bit_depth_chroma_minus8 < 7);

    READ_BITS_OR_RETURN(1, &sps->qpprime_y_zero_transform_bypass_flag);
    READ_BITS_OR_RETURN(1, &sps->seq_scaling_matrix_present_flag);

    if (sps->seq_scaling_matrix_present_flag) {
      DVLOG(4) << "Scaling matrix present";
      res = ParseSPSScalingLists(sps.get());
      if (res != kOk)
        return res;
    } else {
      FillDefaultSeqScalingLists(sps.get());
    }
  }

  READ_UE_OR_RETURN(&sps->log2_max_frame_num_minus4);
  TRUE_OR_RETURN(sps->log2_max_frame_num_minus4 < 13);

  READ_UE_OR_RETURN(&sps->pic_order_cnt_type);
  TRUE_OR_RETURN(sps->pic_order_cnt_type < 3);

  if (sps->pic_order_cnt_type == 0) {
    READ_UE_OR_RETURN(&sps->log2_max_pic_order_cnt_lsb_minus4);
    TRUE_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 < 13);
  } else if (sps->pic_order_cnt_type == 1) {
    READ_BITS_OR_RETURN(1, &sps->delta_pic_order_always_zero_flag);
    READ_SE_OR_RETURN(&sps->offset_for_non_ref_pic);
    READ_SE_OR_RETURN(&sps->offset_for_top_to_bottom_field);
    READ_UE_OR_RETURN(&sps->num_ref_frames_in_pic_order_cnt_cycle);
    for (int i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; ++i)
      READ_SE_OR_RETURN(&sps->offset_for_ref_frame[i]);
  }

  READ_UE_OR_RETURN(&sps->max_num_ref_frames);
  READ_BITS_OR_RETURN(1, &sps->gaps_in_frame_num_value_allowed_flag);

  if (sps->gaps_in_frame_num_value_allowed_flag)
    return kUnsupportedStream;

  READ_UE_OR_RETURN(&sps->pic_width_in_mbs_minus1);
  READ_UE_OR_RETURN(&sps->pic_height_in_map_units_minus1);

  READ_BITS_OR_RETURN(1, &sps->frame_mbs_only_flag);
  if (!sps->frame_mbs_only_flag)
    READ_BITS_OR_RETURN(1, &sps->mb_adaptive_frame_field_flag);

  READ_BITS_OR_RETURN(1, &sps->direct_8x8_inference_flag);

  READ_BITS_OR_RETURN(1, &sps->frame_cropping_flag);
  if (sps->frame_cropping_flag) {
    READ_UE_OR_RETURN(&sps->frame_crop_left_offset);
    READ_UE_OR_RETURN(&sps->frame_crop_right_offset);
    READ_UE_OR_RETURN(&sps->frame_crop_top_offset);
    READ_UE_OR_RETURN(&sps->frame_crop_bottom_offset);
  }

  READ_BITS_OR_RETURN(1, &sps->vui_parameters_present_flag);
  if (sps->vui_parameters_present_flag) {
    DVLOG(1) << "VUI parameters present in SPS, ignoring";
  }

  // If an SPS with the same id already exists, replace it.
  *sps_id = sps->seq_parameter_set_id;
  delete active_SPSes_[*sps_id];
  active_SPSes_[*sps_id] = sps.release();

  return kOk;
}

H264Parser::Result H264Parser::ParsePPS(int* pps_id) {
  // See 7.4.2.2.
  const H264SPS* sps;
  Result res;

  *pps_id = -1;

  scoped_ptr<H264PPS> pps(new H264PPS());

  READ_UE_OR_RETURN(&pps->pic_parameter_set_id);
  READ_UE_OR_RETURN(&pps->seq_parameter_set_id);
  TRUE_OR_RETURN(pps->seq_parameter_set_id < 32);

  sps = GetSPS(pps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps);

  READ_BITS_OR_RETURN(1, &pps->entropy_coding_mode_flag);
  READ_BITS_OR_RETURN(1, &pps->bottom_field_pic_order_in_frame_present_flag);

  READ_UE_OR_RETURN(&pps->num_slice_groups_minus1);
  if (pps->num_slice_groups_minus1 > 1) {
    DVLOG(1) << "Slice groups not supported";
    return kUnsupportedStream;
  }

  READ_UE_OR_RETURN(&pps->num_ref_idx_l0_default_active_minus1);
  TRUE_OR_RETURN(pps->num_ref_idx_l0_default_active_minus1 < 32);

  READ_UE_OR_RETURN(&pps->num_ref_idx_l1_default_active_minus1);
  TRUE_OR_RETURN(pps->num_ref_idx_l1_default_active_minus1 < 32);

  READ_BITS_OR_RETURN(1, &pps->weighted_pred_flag);
  READ_BITS_OR_RETURN(2, &pps->weighted_bipred_idc);
  TRUE_OR_RETURN(pps->weighted_bipred_idc < 3);

  READ_SE_OR_RETURN(&pps->pic_init_qp_minus26);
  IN_RANGE_OR_RETURN(pps->pic_init_qp_minus26, -26, 25);

  READ_SE_OR_RETURN(&pps->pic_init_qs_minus26);
  IN_RANGE_OR_RETURN(pps->pic_init_qs_minus26, -26, 25);

  READ_SE_OR_RETURN(&pps->chroma_qp_index_offset);
  IN_RANGE_OR_RETURN(pps->chroma_qp_index_offset, -12, 12);

  READ_BITS_OR_RETURN(1, &pps->deblocking_filter_control_present_flag);
  READ_BITS_OR_RETURN(1, &pps->constrained_intra_pred_flag);
  READ_BITS_OR_RETURN(1, &pps->redundant_pic_cnt_present_flag);

  if (br_.HasMoreRBSPData()) {
    READ_BITS_OR_RETURN(1, &pps->transform_8x8_mode_flag);
    READ_BITS_OR_RETURN(1, &pps->pic_scaling_matrix_present_flag);

    if (pps->pic_scaling_matrix_present_flag) {
      DVLOG(4) << "Picture scaling matrix present";
      res = ParsePPSScalingLists(*sps, pps.get());
      if (res != kOk)
        return res;
    }

    READ_SE_OR_RETURN(&pps->second_chroma_qp_index_offset);
  }

  // If a PPS with the same id already exists, replace it.
  *pps_id = pps->pic_parameter_set_id;
  delete active_PPSes_[*pps_id];
  active_PPSes_[*pps_id] = pps.release();

  return kOk;
}

H264Parser::Result H264Parser::ParseRefPicListModification(
    int num_ref_idx_active_minus1,
    H264ModificationOfPicNum* ref_list_mods) {
  H264ModificationOfPicNum* pic_num_mod;

  if (num_ref_idx_active_minus1 >= 32)
    return kInvalidStream;

  for (int i = 0; i < 32; ++i) {
    pic_num_mod = &ref_list_mods[i];
    READ_UE_OR_RETURN(&pic_num_mod->modification_of_pic_nums_idc);
    TRUE_OR_RETURN(pic_num_mod->modification_of_pic_nums_idc < 4);

    switch (pic_num_mod->modification_of_pic_nums_idc) {
      case 0:
      case 1:
        READ_UE_OR_RETURN(&pic_num_mod->abs_diff_pic_num_minus1);
        break;

      case 2:
        READ_UE_OR_RETURN(&pic_num_mod->long_term_pic_num);
        break;

      case 3:
        // Per spec, list cannot be empty.
        if (i == 0)
          return kInvalidStream;
        return kOk;

      default:
        return kInvalidStream;
    }
  }

  // If we got here, we didn't get loop end marker prematurely,
  // so make sure it is there for our client.
  int modification_of_pic_nums_idc;
  READ_UE_OR_RETURN(&modification_of_pic_nums_idc);
  TRUE_OR_RETURN(modification_of_pic_nums_idc == 3);

  return kOk;
}

H264Parser::Result H264Parser::ParseRefPicListModifications(
    H264SliceHeader* shdr) {
  Result res;

  if (!shdr->IsISlice() && !shdr->IsSISlice()) {
    READ_BITS_OR_RETURN(1, &shdr->ref_pic_list_modification_flag_l0);
    if (shdr->ref_pic_list_modification_flag_l0) {
      res = ParseRefPicListModification(shdr->num_ref_idx_l0_active_minus1,
                                        shdr->ref_list_l0_modifications);
      if (res != kOk)
        return res;
    }
  }

  if (shdr->IsBSlice()) {
    READ_BITS_OR_RETURN(1, &shdr->ref_pic_list_modification_flag_l1);
    if (shdr->ref_pic_list_modification_flag_l1) {
      res = ParseRefPicListModification(shdr->num_ref_idx_l1_active_minus1,
                                        shdr->ref_list_l1_modifications);
      if (res != kOk)
        return res;
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseWeightingFactors(
    int num_ref_idx_active_minus1,
    int chroma_array_type,
    int luma_log2_weight_denom,
    int chroma_log2_weight_denom,
    H264WeightingFactors* w_facts) {

  int def_luma_weight = 1 << luma_log2_weight_denom;
  int def_chroma_weight = 1 << chroma_log2_weight_denom;

  for (int i = 0; i < num_ref_idx_active_minus1 + 1; ++i) {
    READ_BITS_OR_RETURN(1, &w_facts->luma_weight_flag);
    if (w_facts->luma_weight_flag) {
      READ_SE_OR_RETURN(&w_facts->luma_weight[i]);
      IN_RANGE_OR_RETURN(w_facts->luma_weight[i], -128, 127);

      READ_SE_OR_RETURN(&w_facts->luma_offset[i]);
      IN_RANGE_OR_RETURN(w_facts->luma_offset[i], -128, 127);
    } else {
      w_facts->luma_weight[i] = def_luma_weight;
      w_facts->luma_offset[i] = 0;
    }

    if (chroma_array_type != 0) {
      READ_BITS_OR_RETURN(1, &w_facts->chroma_weight_flag);
      if (w_facts->chroma_weight_flag) {
        for (int j = 0; j < 2; ++j) {
          READ_SE_OR_RETURN(&w_facts->chroma_weight[i][j]);
          IN_RANGE_OR_RETURN(w_facts->chroma_weight[i][j], -128, 127);

          READ_SE_OR_RETURN(&w_facts->chroma_offset[i][j]);
          IN_RANGE_OR_RETURN(w_facts->chroma_offset[i][j], -128, 127);
        }
      } else {
        for (int j = 0; j < 2; ++j) {
          w_facts->chroma_weight[i][j] = def_chroma_weight;
          w_facts->chroma_offset[i][j] = 0;
        }
      }
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParsePredWeightTable(const H264SPS& sps,
                                                    H264SliceHeader* shdr) {
  READ_UE_OR_RETURN(&shdr->luma_log2_weight_denom);
  TRUE_OR_RETURN(shdr->luma_log2_weight_denom < 8);

  if (sps.chroma_array_type != 0)
    READ_UE_OR_RETURN(&shdr->chroma_log2_weight_denom);
  TRUE_OR_RETURN(shdr->chroma_log2_weight_denom < 8);

  Result res = ParseWeightingFactors(shdr->num_ref_idx_l0_active_minus1,
                                     sps.chroma_array_type,
                                     shdr->luma_log2_weight_denom,
                                     shdr->chroma_log2_weight_denom,
                                     &shdr->pred_weight_table_l0);
  if (res != kOk)
    return res;

  if (shdr->IsBSlice()) {
    res = ParseWeightingFactors(shdr->num_ref_idx_l1_active_minus1,
                                sps.chroma_array_type,
                                shdr->luma_log2_weight_denom,
                                shdr->chroma_log2_weight_denom,
                                &shdr->pred_weight_table_l1);
    if (res != kOk)
      return res;
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseDecRefPicMarking(H264SliceHeader *shdr) {
  if (shdr->idr_pic_flag) {
    READ_BITS_OR_RETURN(1, &shdr->no_output_of_prior_pics_flag);
    READ_BITS_OR_RETURN(1, &shdr->long_term_reference_flag);
  } else {
    READ_BITS_OR_RETURN(1, &shdr->adaptive_ref_pic_marking_mode_flag);

    H264DecRefPicMarking* marking;
    if (shdr->adaptive_ref_pic_marking_mode_flag) {
      size_t i;
      for (i = 0; i < arraysize(shdr->ref_pic_marking); ++i) {
        marking = &shdr->ref_pic_marking[i];

        READ_UE_OR_RETURN(&marking->memory_mgmnt_control_operation);
        if (marking->memory_mgmnt_control_operation == 0)
          break;

        if (marking->memory_mgmnt_control_operation == 1 ||
            marking->memory_mgmnt_control_operation == 3)
          READ_UE_OR_RETURN(&marking->difference_of_pic_nums_minus1);

        if (marking->memory_mgmnt_control_operation == 2)
          READ_UE_OR_RETURN(&marking->long_term_pic_num);

        if (marking->memory_mgmnt_control_operation == 3 ||
            marking->memory_mgmnt_control_operation == 6)
          READ_UE_OR_RETURN(&marking->long_term_frame_idx);

        if (marking->memory_mgmnt_control_operation == 4)
          READ_UE_OR_RETURN(&marking->max_long_term_frame_idx_plus1);

        if (marking->memory_mgmnt_control_operation > 6)
          return kInvalidStream;
      }

      if (i == arraysize(shdr->ref_pic_marking)) {
        DVLOG(1) << "Ran out of dec ref pic marking fields";
        return kUnsupportedStream;
      }
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseSliceHeader(const H264NALU& nalu,
                                                H264SliceHeader* shdr) {
  // See 7.4.3.
  const H264SPS* sps;
  const H264PPS* pps;
  Result res;

  memset(shdr, 0, sizeof(*shdr));

  shdr->idr_pic_flag = ((nalu.nal_unit_type == 5) ? true : false);
  shdr->nal_ref_idc = nalu.nal_ref_idc;
  shdr->nalu_data = nalu.data;
  shdr->nalu_size = nalu.size;

  READ_UE_OR_RETURN(&shdr->first_mb_in_slice);
  READ_UE_OR_RETURN(&shdr->slice_type);
  TRUE_OR_RETURN(shdr->slice_type < 10);

  READ_UE_OR_RETURN(&shdr->pic_parameter_set_id);

  pps = GetPPS(shdr->pic_parameter_set_id);
  TRUE_OR_RETURN(pps);

  sps = GetSPS(pps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps);

  if (sps->separate_colour_plane_flag) {
    DVLOG(1) << "Interlaced streams not supported";
    return kUnsupportedStream;
  }

  READ_BITS_OR_RETURN(sps->log2_max_frame_num_minus4 + 4,
                      &shdr->frame_num);
  if (!sps->frame_mbs_only_flag) {
    READ_BITS_OR_RETURN(1, &shdr->field_pic_flag);
    if (shdr->field_pic_flag) {
      DVLOG(1) << "Interlaced streams not supported";
      return kUnsupportedStream;
    }
  }

  if (shdr->idr_pic_flag)
    READ_UE_OR_RETURN(&shdr->idr_pic_id);

  if (sps->pic_order_cnt_type == 0) {
    READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                        &shdr->pic_order_cnt_lsb);
    if (pps->bottom_field_pic_order_in_frame_present_flag &&
        !shdr->field_pic_flag)
      READ_SE_OR_RETURN(&shdr->delta_pic_order_cnt_bottom);
  }

  if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) {
    READ_SE_OR_RETURN(&shdr->delta_pic_order_cnt[0]);
    if (pps->bottom_field_pic_order_in_frame_present_flag &&
        !shdr->field_pic_flag)
      READ_SE_OR_RETURN(&shdr->delta_pic_order_cnt[1]);
  }

  if (pps->redundant_pic_cnt_present_flag) {
    READ_UE_OR_RETURN(&shdr->redundant_pic_cnt);
    TRUE_OR_RETURN(shdr->redundant_pic_cnt < 128);
  }

  if (shdr->IsBSlice())
    READ_BITS_OR_RETURN(1, &shdr->direct_spatial_mv_pred_flag);

  if (shdr->IsPSlice() || shdr->IsSPSlice() || shdr->IsBSlice()) {
    READ_BITS_OR_RETURN(1, &shdr->num_ref_idx_active_override_flag);
    if (shdr->num_ref_idx_active_override_flag) {
      READ_UE_OR_RETURN(&shdr->num_ref_idx_l0_active_minus1);
      if (shdr->IsBSlice())
        READ_UE_OR_RETURN(&shdr->num_ref_idx_l1_active_minus1);
    } else {
      shdr->num_ref_idx_l0_active_minus1 =
            pps->num_ref_idx_l0_default_active_minus1;
      shdr->num_ref_idx_l1_active_minus1 =
            pps->num_ref_idx_l1_default_active_minus1;
    }
  }
  if (shdr->field_pic_flag) {
    TRUE_OR_RETURN(shdr->num_ref_idx_l0_active_minus1 < 32);
    TRUE_OR_RETURN(shdr->num_ref_idx_l1_active_minus1 < 32);
  } else {
    TRUE_OR_RETURN(shdr->num_ref_idx_l0_active_minus1 < 16);
    TRUE_OR_RETURN(shdr->num_ref_idx_l1_active_minus1 < 16);
  }

  if (nalu.nal_unit_type == H264NALU::kCodedSliceExtension) {
    return kUnsupportedStream;
  } else {
    res = ParseRefPicListModifications(shdr);
    if (res != kOk)
      return res;
  }

  if ((pps->weighted_pred_flag && (shdr->IsPSlice() || shdr->IsSPSlice())) ||
      (pps->weighted_bipred_idc == 1 && shdr->IsBSlice())) {
    res = ParsePredWeightTable(*sps, shdr);
    if (res != kOk)
      return res;
  }

  if (nalu.nal_ref_idc != 0) {
    res = ParseDecRefPicMarking(shdr);
    if (res != kOk)
      return res;
  }

  if (pps->entropy_coding_mode_flag &&
      !shdr->IsISlice() && !shdr->IsSISlice()) {
    READ_UE_OR_RETURN(&shdr->cabac_init_idc);
    TRUE_OR_RETURN(shdr->cabac_init_idc < 3);
  }

  READ_SE_OR_RETURN(&shdr->slice_qp_delta);

  if (shdr->IsSPSlice() || shdr->IsSISlice()) {
    if (shdr->IsSPSlice())
      READ_BITS_OR_RETURN(1, &shdr->sp_for_switch_flag);
    READ_SE_OR_RETURN(&shdr->slice_qs_delta);
  }

  if (pps->deblocking_filter_control_present_flag) {
    READ_UE_OR_RETURN(&shdr->disable_deblocking_filter_idc);
    TRUE_OR_RETURN(shdr->disable_deblocking_filter_idc < 3);

    if (shdr->disable_deblocking_filter_idc != 1) {
      READ_SE_OR_RETURN(&shdr->slice_alpha_c0_offset_div2);
      IN_RANGE_OR_RETURN(shdr->slice_alpha_c0_offset_div2, -6, 6);

      READ_SE_OR_RETURN(&shdr->slice_beta_offset_div2);
      IN_RANGE_OR_RETURN(shdr->slice_beta_offset_div2, -6, 6);
    }
  }

  if (pps->num_slice_groups_minus1 > 0) {
    DVLOG(1) << "Slice groups not supported";
    return kUnsupportedStream;
  }

  shdr->header_bit_size = shdr->nalu_size * 8 - br_.NumBitsLeft();

  return kOk;
}

H264Parser::Result H264Parser::ParseSEI(H264SEIMessage* sei_msg) {
  int byte;

  memset(sei_msg, 0, sizeof(*sei_msg));

  READ_BITS_OR_RETURN(8, &byte);
  while (byte == 0xff) {
    sei_msg->type += 255;
    READ_BITS_OR_RETURN(8, &byte);
  }
  sei_msg->type += byte;

  while (byte == 0xff) {
    sei_msg->payload_size += 255;
    READ_BITS_OR_RETURN(8, &byte);
  }
  sei_msg->payload_size += byte;

  DVLOG(4) << "Found SEI message type: " << sei_msg->type
           << " payload size: " << sei_msg->payload_size;

  switch (sei_msg->type) {
    case H264SEIMessage::kSEIRecoveryPoint:
      READ_UE_OR_RETURN(&sei_msg->recovery_point.recovery_frame_cnt);
      READ_BITS_OR_RETURN(1, &sei_msg->recovery_point.exact_match_flag);
      READ_BITS_OR_RETURN(1, &sei_msg->recovery_point.broken_link_flag);
      READ_BITS_OR_RETURN(2,
          &sei_msg->recovery_point.changing_slice_group_idc);
      break;

    default:
      DVLOG(4) << "Unsupported SEI message";
      break;
  }

  return kOk;
}

}  // namespace content
