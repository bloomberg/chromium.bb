// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H264 Annex-B video stream parser.

#ifndef CONTENT_COMMON_GPU_MEDIA_H264_PARSER_H_
#define CONTENT_COMMON_GPU_MEDIA_H264_PARSER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace content {

// For explanations of each struct and its members, see H.264 specification
// at http://www.itu.int/rec/T-REC-H.264.
struct H264NALU {
  H264NALU();

  enum Type {
    kUnspecified = 0,
    kNonIDRSlice = 1,
    kIDRSlice = 5,
    kSEIMessage = 6,
    kSPS = 7,
    kPPS = 8,
    kEOSeq = 9,
    kEOStream = 11,
    kCodedSliceExtension = 20,
  };

  // After (without) start code; we don't own the underlying memory
  // and a shallow copy should be made when copying this struct.
  const uint8* data;
  off_t size;  // From after start code to start code of next NALU (or EOS).

  int nal_ref_idc;
  int nal_unit_type;
};

struct H264SPS {
  H264SPS();

  int profile_idc;
  int level_idc;
  int seq_parameter_set_id;

  int chroma_format_idc;
  bool separate_colour_plane_flag;
  int bit_depth_luma_minus8;
  int bit_depth_chroma_minus8;
  bool qpprime_y_zero_transform_bypass_flag;

  bool seq_scaling_matrix_present_flag;
  // Changing types below will break assumptions in code that fills
  // default scaling lists.
  int scaling_list4x4[6][16];
  int scaling_list8x8[6][64];

  int log2_max_frame_num_minus4;
  int pic_order_cnt_type;
  int log2_max_pic_order_cnt_lsb_minus4;
  bool delta_pic_order_always_zero_flag;
  int offset_for_non_ref_pic;
  int offset_for_top_to_bottom_field;
  int num_ref_frames_in_pic_order_cnt_cycle;
  int offset_for_ref_frame[255];
  int max_num_ref_frames;
  bool gaps_in_frame_num_value_allowed_flag;
  int pic_width_in_mbs_minus1;
  int pic_height_in_map_units_minus1;
  bool frame_mbs_only_flag;
  bool mb_adaptive_frame_field_flag;
  bool direct_8x8_inference_flag;
  bool frame_cropping_flag;
  int frame_crop_left_offset;
  int frame_crop_right_offset;
  int frame_crop_top_offset;
  int frame_crop_bottom_offset;
  bool vui_parameters_present_flag;
  int chroma_array_type;
};

struct H264PPS {
  H264PPS();

  int pic_parameter_set_id;
  int seq_parameter_set_id;
  bool entropy_coding_mode_flag;
  bool bottom_field_pic_order_in_frame_present_flag;
  int num_slice_groups_minus1;
  // TODO(posciak): Slice groups not implemented, could be added at some point.
  int num_ref_idx_l0_default_active_minus1;
  int num_ref_idx_l1_default_active_minus1;
  bool weighted_pred_flag;
  int weighted_bipred_idc;
  int pic_init_qp_minus26;
  int pic_init_qs_minus26;
  int chroma_qp_index_offset;
  bool deblocking_filter_control_present_flag;
  bool constrained_intra_pred_flag;
  bool redundant_pic_cnt_present_flag;
  bool transform_8x8_mode_flag;

  bool pic_scaling_matrix_present_flag;
  int scaling_list4x4[6][16];
  int scaling_list8x8[6][64];

  int second_chroma_qp_index_offset;
};

struct H264ModificationOfPicNum {
  int modification_of_pic_nums_idc;
  union {
    int abs_diff_pic_num_minus1;
    int long_term_pic_num;
  };
};

struct H264WeightingFactors {
  bool luma_weight_flag;
  bool chroma_weight_flag;
  int luma_weight[32];
  int luma_offset[32];
  int chroma_weight[32][2];
  int chroma_offset[32][2];
};

struct H264DecRefPicMarking {
  int memory_mgmnt_control_operation;
  int difference_of_pic_nums_minus1;
  int long_term_pic_num;
  int long_term_frame_idx;
  int max_long_term_frame_idx_plus1;
};

struct H264SliceHeader {
  H264SliceHeader();

  enum {
    kRefListSize = 32,
    kRefListModSize = kRefListSize
  };

  enum Type {
    kPSlice = 0,
    kBSlice = 1,
    kISlice = 2,
    kSPSlice = 3,
    kSISlice = 4,
  };

  bool IsPSlice() const;
  bool IsBSlice() const;
  bool IsISlice() const;
  bool IsSPSlice() const;
  bool IsSISlice() const;

  bool idr_pic_flag;  // from NAL header
  int nal_ref_idc;  // from NAL header
  const uint8* nalu_data;  // from NAL header
  off_t nalu_size;  // from NAL header
  off_t header_bit_size;  // calculated

  int first_mb_in_slice;
  int slice_type;
  int pic_parameter_set_id;
  int colour_plane_id;
  int frame_num;
  bool field_pic_flag;
  bool bottom_field_flag;
  int idr_pic_id;
  int pic_order_cnt_lsb;
  int delta_pic_order_cnt_bottom;
  int delta_pic_order_cnt[2];
  int redundant_pic_cnt;
  bool direct_spatial_mv_pred_flag;

  bool num_ref_idx_active_override_flag;
  int num_ref_idx_l0_active_minus1;
  int num_ref_idx_l1_active_minus1;
  bool ref_pic_list_modification_flag_l0;
  bool ref_pic_list_modification_flag_l1;
  H264ModificationOfPicNum ref_list_l0_modifications[kRefListModSize];
  H264ModificationOfPicNum ref_list_l1_modifications[kRefListModSize];

  int luma_log2_weight_denom;
  int chroma_log2_weight_denom;

  bool luma_weight_l0_flag;
  bool chroma_weight_l0_flag;
  H264WeightingFactors pred_weight_table_l0;

  bool luma_weight_l1_flag;
  bool chroma_weight_l1_flag;
  H264WeightingFactors pred_weight_table_l1;

  bool no_output_of_prior_pics_flag;
  bool long_term_reference_flag;

  bool adaptive_ref_pic_marking_mode_flag;
  H264DecRefPicMarking ref_pic_marking[kRefListSize];

  int cabac_init_idc;
  int slice_qp_delta;
  bool sp_for_switch_flag;
  int slice_qs_delta;
  int disable_deblocking_filter_idc;
  int slice_alpha_c0_offset_div2;
  int slice_beta_offset_div2;
};

struct H264SEIRecoveryPoint {
  int recovery_frame_cnt;
  bool exact_match_flag;
  bool broken_link_flag;
  int changing_slice_group_idc;
};

struct H264SEIMessage {
  H264SEIMessage();

  enum Type {
    kSEIRecoveryPoint = 6,
  };

  int type;
  int payload_size;
  union {
    // Placeholder; in future more supported types will contribute to more
    // union members here.
    H264SEIRecoveryPoint recovery_point;
  };
};

// Class to parse an Annex-B H.264 stream,
// as specified in chapters 7 and Annex B of the H.264 spec.
class H264Parser {
 public:
  enum Result {
    kOk,
    kInvalidStream,     // error in stream
    kUnsupportedStream, // stream not supported by the parser
    kEOStream,          // end of stream
  };

  H264Parser();
  ~H264Parser();

  void Reset();
  // Set current stream pointer to |stream| of |stream_size| in bytes,
  // |stream| owned by caller.
  void SetStream(const uint8* stream, off_t stream_size);

  // Read the stream to find the next NALU, identify it and return
  // that information in |*nalu|. This advances the stream to the beginning
  // of this NALU, but not past it, so subsequent calls to NALU-specific
  // parsing functions (ParseSPS, etc.)  will parse this NALU.
  // If the caller wishes to skip the current NALU, it can call this function
  // again, instead of any NALU-type specific parse functions below.
  Result AdvanceToNextNALU(H264NALU* nalu);

  // NALU-specific parsing functions.
  // These should be called after AdvanceToNextNALU().

  // SPSes and PPSes are owned by the parser class and the memory for their
  // structures is managed here, not by the caller, as they are reused
  // across NALUs.
  //
  // Parse an SPS/PPS NALU and save their data in the parser, returning id
  // of the parsed structure in |*pps_id|/|*sps_id|.
  // To get a pointer to a given SPS/PPS structure, use GetSPS()/GetPPS(),
  // passing the returned |*sps_id|/|*pps_id| as parameter.
  // TODO(posciak,fischman): consider replacing returning Result from Parse*()
  // methods with a scoped_ptr and adding an AtEOS() function to check for EOS
  // if Parse*() return NULL.
  Result ParseSPS(int* sps_id);
  Result ParsePPS(int* pps_id);

  // Return a pointer to SPS/PPS with given |sps_id|/|pps_id| or NULL if not
  // present.
  const H264SPS* GetSPS(int sps_id);
  const H264PPS* GetPPS(int pps_id);

  // Slice headers and SEI messages are not used across NALUs by the parser
  // and can be discarded after current NALU, so the parser does not store
  // them, nor does it manage their memory.
  // The caller has to provide and manage it instead.

  // Parse a slice header, returning it in |*shdr|. |*nalu| must be set to
  // the NALU returned from AdvanceToNextNALU() and corresponding to |*shdr|.
  Result ParseSliceHeader(const H264NALU& nalu, H264SliceHeader* shdr);

  // Parse a SEI message, returning it in |*sei_msg|, provided and managed
  // by the caller.
  Result ParseSEI(H264SEIMessage* sei_msg);

 private:
  // A class to provide bit-granularity reading of H.264 streams.
  // This is not a generic bit reader class, as it takes into account
  // H.264 stream-specific constraints, such as skipping emulation-prevention
  // bytes and stop bits. See spec for more details.
  // TODO(posciak): need separate unittests for this class.
  class H264BitReader {
   public:
    H264BitReader();
    ~H264BitReader();

    // Initialize the reader to start reading at |data|, |size| being size
    // of |data| in bytes.
    // Return false on insufficient size of stream..
    // TODO(posciak,fischman): consider replacing Initialize() with
    // heap-allocating and creating bit readers on demand instead.
    bool Initialize(const uint8* data, off_t size);

    // Read |num_bits| next bits from stream and return in |*out|, first bit
    // from the stream starting at |num_bits| position in |*out|.
    // |num_bits| may be 1-32, inclusive.
    // Return false if the given number of bits cannot be read (not enough
    // bits in the stream), true otherwise.
    bool ReadBits(int num_bits, int *out);

    // Return the number of bits left in the stream.
    off_t NumBitsLeft();

    // See the definition of more_rbsp_data() in spec.
    bool HasMoreRBSPData();

   private:
    // Advance to the next byte, loading it into curr_byte_.
    // Return false on end of stream.
    bool UpdateCurrByte();

    // Pointer to the next unread (not in curr_byte_) byte in the stream.
    const uint8* data_;

    // Bytes left in the stream (without the curr_byte_).
    off_t bytes_left_;

    // Contents of the current byte; first unread bit starting at position
    // 8 - num_remaining_bits_in_curr_byte_ from MSB.
    int curr_byte_;

    // Number of bits remaining in curr_byte_
    int num_remaining_bits_in_curr_byte_;

    // Used in emulation prevention three byte detection (see spec).
    // Initially set to 0xffff to accept all initial two-byte sequences.
    int prev_two_bytes_;

    DISALLOW_COPY_AND_ASSIGN(H264BitReader);
  };

  // Exp-Golomb code parsing as specified in chapter 9.1 of the spec.
  // Read one unsigned exp-Golomb code from the stream and return in |*val|.
  Result ReadUE(int* val);

  // Read one signed exp-Golomb code from the stream and return in |*val|.
  Result ReadSE(int* val);

  // Parse scaling lists (see spec).
  Result ParseScalingList(int size, int* scaling_list, bool* use_default);
  Result ParseSPSScalingLists(H264SPS* sps);
  Result ParsePPSScalingLists(const H264SPS& sps, H264PPS* pps);

  // Parse reference picture lists' modifications (see spec).
  Result ParseRefPicListModifications(H264SliceHeader* shdr);
  Result ParseRefPicListModification(int num_ref_idx_active_minus1,
                                     H264ModificationOfPicNum* ref_list_mods);

  // Parse prediction weight table (see spec).
  Result ParsePredWeightTable(const H264SPS& sps, H264SliceHeader* shdr);

  // Parse weighting factors (see spec).
  Result ParseWeightingFactors(int num_ref_idx_active_minus1,
                               int chroma_array_type,
                               int luma_log2_weight_denom,
                               int chroma_log2_weight_denom,
                               H264WeightingFactors* w_facts);

  // Parse decoded reference picture marking information (see spec).
  Result ParseDecRefPicMarking(H264SliceHeader *shdr);

  // Pointer to the current NALU in the stream.
  const uint8* stream_;

  // Bytes left in the stream after the current NALU.
  off_t bytes_left_;

  H264BitReader br_;

  // PPSes and SPSes stored for future reference.
  typedef std::map<int, H264SPS*> SPSById;
  typedef std::map<int, H264PPS*> PPSById;
  SPSById active_SPSes_;
  PPSById active_PPSes_;

  DISALLOW_COPY_AND_ASSIGN(H264Parser);
};

} // namespace content

#endif // CONTENT_COMMON_GPU_MEDIA_H264_PARSER_H_

