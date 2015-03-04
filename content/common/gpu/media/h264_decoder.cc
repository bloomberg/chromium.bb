// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "content/common/gpu/media/h264_decoder.h"

namespace content {

H264Decoder::H264Accelerator::H264Accelerator() {
}

H264Decoder::H264Accelerator::~H264Accelerator() {
}

H264Decoder::H264Decoder(H264Accelerator* accelerator)
    : max_pic_order_cnt_lsb_(0),
      max_frame_num_(0),
      max_pic_num_(0),
      max_long_term_frame_idx_(0),
      max_num_reorder_frames_(0),
      curr_sps_id_(-1),
      curr_pps_id_(-1),
      accelerator_(accelerator) {
  DCHECK(accelerator_);
  Reset();
  state_ = kNeedStreamMetadata;
}

H264Decoder::~H264Decoder() {
}

void H264Decoder::Reset() {
  curr_pic_ = nullptr;
  curr_nalu_ = nullptr;
  curr_slice_hdr_ = nullptr;

  frame_num_ = 0;
  prev_frame_num_ = -1;
  prev_frame_num_offset_ = -1;

  prev_ref_has_memmgmnt5_ = false;
  prev_ref_top_field_order_cnt_ = -1;
  prev_ref_pic_order_cnt_msb_ = -1;
  prev_ref_pic_order_cnt_lsb_ = -1;
  prev_ref_field_ = H264Picture::FIELD_NONE;

  ref_pic_list_p0_.clear();
  ref_pic_list_b0_.clear();
  ref_pic_list_b1_.clear();
  dpb_.Clear();
  parser_.Reset();
  accelerator_->Reset();
  last_output_poc_ = std::numeric_limits<int>::min();

  // If we are in kDecoding, we can resume without processing an SPS.
  if (state_ == kDecoding)
    state_ = kAfterReset;
}

void H264Decoder::PrepareRefPicLists(media::H264SliceHeader* slice_hdr) {
  ConstructReferencePicListsP(slice_hdr);
  ConstructReferencePicListsB(slice_hdr);
}

bool H264Decoder::ModifyReferencePicLists(media::H264SliceHeader* slice_hdr,
                                          H264Picture::Vector* ref_pic_list0,
                                          H264Picture::Vector* ref_pic_list1) {
  ref_pic_list0->clear();
  ref_pic_list1->clear();

  // Fill reference picture lists for B and S/SP slices.
  if (slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) {
    *ref_pic_list0 = ref_pic_list_p0_;
    return ModifyReferencePicList(slice_hdr, 0, ref_pic_list0);
  } else if (slice_hdr->IsBSlice()) {
    *ref_pic_list0 = ref_pic_list_b0_;
    *ref_pic_list1 = ref_pic_list_b1_;
    return ModifyReferencePicList(slice_hdr, 0, ref_pic_list0) &&
           ModifyReferencePicList(slice_hdr, 1, ref_pic_list1);
  }

  return true;
}

bool H264Decoder::DecodePicture() {
  DCHECK(curr_pic_.get());

  DVLOG(4) << "Decoding POC " << curr_pic_->pic_order_cnt;
  return accelerator_->SubmitDecode(curr_pic_);
}

bool H264Decoder::InitCurrPicture(media::H264SliceHeader* slice_hdr) {
  DCHECK(curr_pic_.get());

  curr_pic_->idr = slice_hdr->idr_pic_flag;

  if (slice_hdr->field_pic_flag) {
    curr_pic_->field = slice_hdr->bottom_field_flag ? H264Picture::FIELD_BOTTOM
                                                    : H264Picture::FIELD_TOP;
  } else {
    curr_pic_->field = H264Picture::FIELD_NONE;
  }

  curr_pic_->ref = slice_hdr->nal_ref_idc != 0;
  // This assumes non-interlaced stream.
  curr_pic_->frame_num = curr_pic_->pic_num = slice_hdr->frame_num;

  if (!CalculatePicOrderCounts(slice_hdr))
    return false;

  curr_pic_->long_term_reference_flag = slice_hdr->long_term_reference_flag;
  curr_pic_->adaptive_ref_pic_marking_mode_flag =
      slice_hdr->adaptive_ref_pic_marking_mode_flag;

  // If the slice header indicates we will have to perform reference marking
  // process after this picture is decoded, store required data for that
  // purpose.
  if (slice_hdr->adaptive_ref_pic_marking_mode_flag) {
    static_assert(sizeof(curr_pic_->ref_pic_marking) ==
                  sizeof(slice_hdr->ref_pic_marking),
                  "Array sizes of ref pic marking do not match.");
    memcpy(curr_pic_->ref_pic_marking, slice_hdr->ref_pic_marking,
           sizeof(curr_pic_->ref_pic_marking));
  }

  return true;
}

bool H264Decoder::CalculatePicOrderCounts(media::H264SliceHeader* slice_hdr) {
  DCHECK_NE(curr_sps_id_, -1);
  const media::H264SPS* sps = parser_.GetSPS(curr_sps_id_);

  int pic_order_cnt_lsb = slice_hdr->pic_order_cnt_lsb;
  curr_pic_->pic_order_cnt_lsb = pic_order_cnt_lsb;

  switch (sps->pic_order_cnt_type) {
    case 0:
      // See spec 8.2.1.1.
      int prev_pic_order_cnt_msb, prev_pic_order_cnt_lsb;
      if (slice_hdr->idr_pic_flag) {
        prev_pic_order_cnt_msb = prev_pic_order_cnt_lsb = 0;
      } else {
        if (prev_ref_has_memmgmnt5_) {
          if (prev_ref_field_ != H264Picture::FIELD_BOTTOM) {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = prev_ref_top_field_order_cnt_;
          } else {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = 0;
          }
        } else {
          prev_pic_order_cnt_msb = prev_ref_pic_order_cnt_msb_;
          prev_pic_order_cnt_lsb = prev_ref_pic_order_cnt_lsb_;
        }
      }

      DCHECK_NE(max_pic_order_cnt_lsb_, 0);
      if ((pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - pic_order_cnt_lsb >=
           max_pic_order_cnt_lsb_ / 2)) {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb +
          max_pic_order_cnt_lsb_;
      } else if ((pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
          (pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
           max_pic_order_cnt_lsb_ / 2)) {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb -
          max_pic_order_cnt_lsb_;
      } else {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      if (curr_pic_->field != H264Picture::FIELD_BOTTOM) {
        curr_pic_->top_field_order_cnt = curr_pic_->pic_order_cnt_msb +
          pic_order_cnt_lsb;
      }

      if (curr_pic_->field != H264Picture::FIELD_TOP) {
        // TODO posciak: perhaps replace with pic->field?
        if (!slice_hdr->field_pic_flag) {
          curr_pic_->bottom_field_order_cnt = curr_pic_->top_field_order_cnt +
            slice_hdr->delta_pic_order_cnt_bottom;
        } else {
          curr_pic_->bottom_field_order_cnt = curr_pic_->pic_order_cnt_msb +
            pic_order_cnt_lsb;
        }
      }
      break;

    case 1: {
      // See spec 8.2.1.2.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (slice_hdr->idr_pic_flag)
        curr_pic_->frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr->frame_num)
        curr_pic_->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        curr_pic_->frame_num_offset = prev_frame_num_offset_;

      int abs_frame_num = 0;
      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
        abs_frame_num = curr_pic_->frame_num_offset + slice_hdr->frame_num;
      else
        abs_frame_num = 0;

      if (slice_hdr->nal_ref_idc == 0 && abs_frame_num > 0)
        --abs_frame_num;

      int expected_pic_order_cnt = 0;
      if (abs_frame_num > 0) {
        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          DVLOG(1) << "Invalid num_ref_frames_in_pic_order_cnt_cycle "
                   << "in stream";
          return false;
        }

        int pic_order_cnt_cycle_cnt = (abs_frame_num - 1) /
            sps->num_ref_frames_in_pic_order_cnt_cycle;
        int frame_num_in_pic_order_cnt_cycle = (abs_frame_num - 1) %
            sps->num_ref_frames_in_pic_order_cnt_cycle;

        expected_pic_order_cnt = pic_order_cnt_cycle_cnt *
            sps->expected_delta_per_pic_order_cnt_cycle;
        // frame_num_in_pic_order_cnt_cycle is verified < 255 in parser
        for (int i = 0; i <= frame_num_in_pic_order_cnt_cycle; ++i)
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
      }

      if (!slice_hdr->nal_ref_idc)
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;

      if (!slice_hdr->field_pic_flag) {
        curr_pic_->top_field_order_cnt = expected_pic_order_cnt +
            slice_hdr->delta_pic_order_cnt0;
        curr_pic_->bottom_field_order_cnt = curr_pic_->top_field_order_cnt +
            sps->offset_for_top_to_bottom_field +
            slice_hdr->delta_pic_order_cnt1;
      } else if (!slice_hdr->bottom_field_flag) {
        curr_pic_->top_field_order_cnt = expected_pic_order_cnt +
            slice_hdr->delta_pic_order_cnt0;
      } else {
        curr_pic_->bottom_field_order_cnt = expected_pic_order_cnt +
            sps->offset_for_top_to_bottom_field +
            slice_hdr->delta_pic_order_cnt0;
      }
      break;
    }

    case 2:
      // See spec 8.2.1.3.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (slice_hdr->idr_pic_flag)
        curr_pic_->frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr->frame_num)
        curr_pic_->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        curr_pic_->frame_num_offset = prev_frame_num_offset_;

      int temp_pic_order_cnt;
      if (slice_hdr->idr_pic_flag) {
        temp_pic_order_cnt = 0;
      } else if (!slice_hdr->nal_ref_idc) {
        temp_pic_order_cnt =
            2 * (curr_pic_->frame_num_offset + slice_hdr->frame_num) - 1;
      } else {
        temp_pic_order_cnt = 2 * (curr_pic_->frame_num_offset +
            slice_hdr->frame_num);
      }

      if (!slice_hdr->field_pic_flag) {
        curr_pic_->top_field_order_cnt = temp_pic_order_cnt;
        curr_pic_->bottom_field_order_cnt = temp_pic_order_cnt;
      } else if (slice_hdr->bottom_field_flag) {
        curr_pic_->bottom_field_order_cnt = temp_pic_order_cnt;
      } else {
        curr_pic_->top_field_order_cnt = temp_pic_order_cnt;
      }
      break;

    default:
      DVLOG(1) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return false;
  }

  switch (curr_pic_->field) {
    case H264Picture::FIELD_NONE:
      curr_pic_->pic_order_cnt = std::min(curr_pic_->top_field_order_cnt,
                                          curr_pic_->bottom_field_order_cnt);
      break;
    case H264Picture::FIELD_TOP:
      curr_pic_->pic_order_cnt = curr_pic_->top_field_order_cnt;
      break;
    case H264Picture::FIELD_BOTTOM:
      curr_pic_->pic_order_cnt = curr_pic_->bottom_field_order_cnt;
      break;
  }

  return true;
}

void H264Decoder::UpdatePicNums() {
  for (auto& pic : dpb_) {
    if (!pic->ref)
      continue;

    // Below assumes non-interlaced stream.
    DCHECK_EQ(pic->field, H264Picture::FIELD_NONE);
    if (pic->long_term) {
      pic->long_term_pic_num = pic->long_term_frame_idx;
    } else {
      if (pic->frame_num > frame_num_)
        pic->frame_num_wrap = pic->frame_num - max_frame_num_;
      else
        pic->frame_num_wrap = pic->frame_num;

      pic->pic_num = pic->frame_num_wrap;
    }
  }
}

struct PicNumDescCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->pic_num > b->pic_num;
  }
};

struct LongTermPicNumAscCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->long_term_pic_num < b->long_term_pic_num;
  }
};

void H264Decoder::ConstructReferencePicListsP(
    media::H264SliceHeader* slice_hdr) {
  // RefPicList0 (8.2.4.2.1) [[1] [2]], where:
  // [1] shortterm ref pics sorted by descending pic_num,
  // [2] longterm ref pics by ascending long_term_pic_num.
  ref_pic_list_p0_.clear();

  // First get the short ref pics...
  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_p0_);
  size_t num_short_refs = ref_pic_list_p0_.size();

  // and sort them to get [1].
  std::sort(ref_pic_list_p0_.begin(), ref_pic_list_p0_.end(),
            PicNumDescCompare());

  // Now get long term pics and sort them by long_term_pic_num to get [2].
  dpb_.GetLongTermRefPicsAppending(&ref_pic_list_p0_);
  std::sort(ref_pic_list_p0_.begin() + num_short_refs, ref_pic_list_p0_.end(),
            LongTermPicNumAscCompare());

  // Cut off if we have more than requested in slice header.
  ref_pic_list_p0_.resize(slice_hdr->num_ref_idx_l0_active_minus1 + 1);
}

struct POCAscCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->pic_order_cnt < b->pic_order_cnt;
  }
};

struct POCDescCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->pic_order_cnt > b->pic_order_cnt;
  }
};

void H264Decoder::ConstructReferencePicListsB(
    media::H264SliceHeader* slice_hdr) {
  // RefPicList0 (8.2.4.2.3) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC < curr_pic's POC sorted by descending POC,
  // [2] shortterm ref pics with POC > curr_pic's POC by ascending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.
  ref_pic_list_b0_.clear();
  ref_pic_list_b1_.clear();
  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_b0_);
  size_t num_short_refs = ref_pic_list_b0_.size();

  // First sort ascending, this will put [1] in right place and finish [2].
  std::sort(ref_pic_list_b0_.begin(), ref_pic_list_b0_.end(), POCAscCompare());

  // Find first with POC > curr_pic's POC to get first element in [2]...
  H264Picture::Vector::iterator iter;
  iter = std::upper_bound(ref_pic_list_b0_.begin(), ref_pic_list_b0_.end(),
                          curr_pic_.get(), POCAscCompare());

  // and sort [1] descending, thus finishing sequence [1] [2].
  std::sort(ref_pic_list_b0_.begin(), iter, POCDescCompare());

  // Now add [3] and sort by ascending long_term_pic_num.
  dpb_.GetLongTermRefPicsAppending(&ref_pic_list_b0_);
  std::sort(ref_pic_list_b0_.begin() + num_short_refs, ref_pic_list_b0_.end(),
            LongTermPicNumAscCompare());

  // RefPicList1 (8.2.4.2.4) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC > curr_pic's POC sorted by ascending POC,
  // [2] shortterm ref pics with POC < curr_pic's POC by descending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.

  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_b1_);
  num_short_refs = ref_pic_list_b1_.size();

  // First sort by descending POC.
  std::sort(ref_pic_list_b1_.begin(), ref_pic_list_b1_.end(), POCDescCompare());

  // Find first with POC < curr_pic's POC to get first element in [2]...
  iter = std::upper_bound(ref_pic_list_b1_.begin(), ref_pic_list_b1_.end(),
                          curr_pic_.get(), POCDescCompare());

  // and sort [1] ascending.
  std::sort(ref_pic_list_b1_.begin(), iter, POCAscCompare());

  // Now add [3] and sort by ascending long_term_pic_num
  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_b1_);
  std::sort(ref_pic_list_b1_.begin() + num_short_refs, ref_pic_list_b1_.end(),
            LongTermPicNumAscCompare());

  // If lists identical, swap first two entries in RefPicList1 (spec 8.2.4.2.3)
  if (ref_pic_list_b1_.size() > 1 &&
      std::equal(ref_pic_list_b0_.begin(), ref_pic_list_b0_.end(),
                 ref_pic_list_b1_.begin()))
    std::swap(ref_pic_list_b1_[0], ref_pic_list_b1_[1]);

  // Per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to indicate
  // there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference.
  ref_pic_list_b0_.resize(slice_hdr->num_ref_idx_l0_active_minus1 + 1);
  ref_pic_list_b1_.resize(slice_hdr->num_ref_idx_l1_active_minus1 + 1);
}

// See 8.2.4
int H264Decoder::PicNumF(const scoped_refptr<H264Picture>& pic) {
  if (!pic)
    return -1;

  if (!pic->long_term)
    return pic->pic_num;
  else
    return max_pic_num_;
}

// See 8.2.4
int H264Decoder::LongTermPicNumF(const scoped_refptr<H264Picture>& pic) {
  if (pic->ref && pic->long_term)
    return pic->long_term_pic_num;
  else
    return 2 * (max_long_term_frame_idx_ + 1);
}

// Shift elements on the |v| starting from |from| to |to|, inclusive,
// one position to the right and insert pic at |from|.
static void ShiftRightAndInsert(H264Picture::Vector* v,
                                int from,
                                int to,
                                const scoped_refptr<H264Picture>& pic) {
  // Security checks, do not disable in Debug mode.
  CHECK(from <= to);
  CHECK(to <= std::numeric_limits<int>::max() - 2);
  // Additional checks. Debug mode ok.
  DCHECK(v);
  DCHECK(pic);
  DCHECK((to + 1 == static_cast<int>(v->size())) ||
         (to + 2 == static_cast<int>(v->size())));

  v->resize(to + 2);

  for (int i = to + 1; i > from; --i)
    (*v)[i] = (*v)[i - 1];

  (*v)[from] = pic;
}

bool H264Decoder::ModifyReferencePicList(media::H264SliceHeader* slice_hdr,
                                         int list,
                                         H264Picture::Vector* ref_pic_listx) {
  int num_ref_idx_lX_active_minus1;
  media::H264ModificationOfPicNum* list_mod;

  // This can process either ref_pic_list0 or ref_pic_list1, depending on
  // the list argument. Set up pointers to proper list to be processed here.
  if (list == 0) {
    if (!slice_hdr->ref_pic_list_modification_flag_l0)
      return true;

    list_mod = slice_hdr->ref_list_l0_modifications;
  } else {
    if (!slice_hdr->ref_pic_list_modification_flag_l1)
      return true;

    list_mod = slice_hdr->ref_list_l1_modifications;
  }

  num_ref_idx_lX_active_minus1 = ref_pic_listx->size() - 1;
  DCHECK_GE(num_ref_idx_lX_active_minus1, 0);

  // Spec 8.2.4.3:
  // Reorder pictures on the list in a way specified in the stream.
  int pic_num_lx_pred = curr_pic_->pic_num;
  int ref_idx_lx = 0;
  int pic_num_lx_no_wrap;
  int pic_num_lx;
  bool done = false;
  scoped_refptr<H264Picture> pic;
  for (int i = 0; i < media::H264SliceHeader::kRefListModSize && !done; ++i) {
    switch (list_mod->modification_of_pic_nums_idc) {
      case 0:
      case 1:
        // Modify short reference picture position.
        if (list_mod->modification_of_pic_nums_idc == 0) {
          // Subtract given value from predicted PicNum.
          pic_num_lx_no_wrap = pic_num_lx_pred -
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes < 0 as result
          // of subtraction.
          if (pic_num_lx_no_wrap < 0)
            pic_num_lx_no_wrap += max_pic_num_;
        } else {
          // Add given value to predicted PicNum.
          pic_num_lx_no_wrap = pic_num_lx_pred +
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes >= max_pic_num_ as result
          // of the addition.
          if (pic_num_lx_no_wrap >= max_pic_num_)
            pic_num_lx_no_wrap -= max_pic_num_;
        }

        // For use in next iteration.
        pic_num_lx_pred = pic_num_lx_no_wrap;

        if (pic_num_lx_no_wrap > curr_pic_->pic_num)
          pic_num_lx = pic_num_lx_no_wrap - max_pic_num_;
        else
          pic_num_lx = pic_num_lx_no_wrap;

        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  media::H264SliceHeader::kRefListModSize);
        pic = dpb_.GetShortRefPicByPicNum(pic_num_lx);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num " << pic_num_lx;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (PicNumF((*ref_pic_listx)[src]) != pic_num_lx)
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 2:
        // Modify long term reference picture position.
        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  media::H264SliceHeader::kRefListModSize);
        pic = dpb_.GetLongRefPicByLongTermPicNum(list_mod->long_term_pic_num);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num "
                   << list_mod->long_term_pic_num;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (LongTermPicNumF((*ref_pic_listx)[src]) !=
              static_cast<int>(list_mod->long_term_pic_num))
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 3:
        // End of modification list.
        done = true;
        break;

      default:
        // May be recoverable.
        DVLOG(1) << "Invalid modification_of_pic_nums_idc="
                 << list_mod->modification_of_pic_nums_idc
                 << " in position " << i;
        break;
    }

    ++list_mod;
  }

  // Per NOTE 2 in 8.2.4.3.2, the ref_pic_listx size in the above loop is
  // temporarily made one element longer than the required final list.
  // Resize the list back to its required size.
  ref_pic_listx->resize(num_ref_idx_lX_active_minus1 + 1);

  return true;
}

void H264Decoder::OutputPic(scoped_refptr<H264Picture> pic) {
  DCHECK(!pic->outputted);
  pic->outputted = true;
  last_output_poc_ = pic->pic_order_cnt;

  DVLOG(4) << "Posting output task for POC: " << pic->pic_order_cnt;
  accelerator_->OutputPicture(pic);
}

void H264Decoder::ClearDPB() {
  // Clear DPB contents, marking the pictures as unused first.
  dpb_.Clear();
  last_output_poc_ = std::numeric_limits<int>::min();
}

bool H264Decoder::OutputAllRemainingPics() {
  // Output all pictures that are waiting to be outputted.
  FinishPrevFrameIfPresent();
  H264Picture::Vector to_output;
  dpb_.GetNotOutputtedPicsAppending(&to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  for (auto& pic : to_output)
    OutputPic(pic);

  return true;
}

bool H264Decoder::Flush() {
  DVLOG(2) << "Decoder flush";

  if (!OutputAllRemainingPics())
    return false;

  ClearDPB();
  DVLOG(2) << "Decoder flush finished";
  return true;
}

bool H264Decoder::StartNewFrame(media::H264SliceHeader* slice_hdr) {
  // TODO posciak: add handling of max_num_ref_frames per spec.
  CHECK(curr_pic_.get());

  if (!InitCurrPicture(slice_hdr))
    return false;

  DCHECK_GT(max_frame_num_, 0);

  UpdatePicNums();
  DCHECK(slice_hdr);
  PrepareRefPicLists(slice_hdr);

  const media::H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);
  const media::H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
  DCHECK(sps);

  if (!accelerator_->SubmitFrameMetadata(sps, pps, dpb_, ref_pic_list_p0_,
                                         ref_pic_list_b0_, ref_pic_list_b1_,
                                         curr_pic_.get()))
    return false;

  return true;
}

bool H264Decoder::HandleMemoryManagementOps() {
  // 8.2.5.4
  for (unsigned int i = 0; i < arraysize(curr_pic_->ref_pic_marking); ++i) {
    // Code below does not support interlaced stream (per-field pictures).
    media::H264DecRefPicMarking* ref_pic_marking =
        &curr_pic_->ref_pic_marking[i];
    scoped_refptr<H264Picture> to_mark;
    int pic_num_x;

    switch (ref_pic_marking->memory_mgmnt_control_operation) {
      case 0:
        // Normal end of operations' specification.
        return true;

      case 1:
        // Mark a short term reference picture as unused so it can be removed
        // if outputted.
        pic_num_x = curr_pic_->pic_num -
                    (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          DVLOG(1) << "Invalid short ref pic num to unmark";
          return false;
        }
        break;

      case 2:
        // Mark a long term reference picture as unused so it can be removed
        // if outputted.
        to_mark = dpb_.GetLongRefPicByLongTermPicNum(
            ref_pic_marking->long_term_pic_num);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          DVLOG(1) << "Invalid long term ref pic num to unmark";
          return false;
        }
        break;

      case 3:
        // Mark a short term reference picture as long term reference.
        pic_num_x = curr_pic_->pic_num -
                    (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          DCHECK(to_mark->ref && !to_mark->long_term);
          to_mark->long_term = true;
          to_mark->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        } else {
          DVLOG(1) << "Invalid short term ref pic num to mark as long ref";
          return false;
        }
        break;

      case 4: {
        // Unmark all reference pictures with long_term_frame_idx over new max.
        max_long_term_frame_idx_ =
            ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
        H264Picture::Vector long_terms;
        dpb_.GetLongTermRefPicsAppending(&long_terms);
        for (size_t i = 0; i < long_terms.size(); ++i) {
          scoped_refptr<H264Picture>& pic = long_terms[i];
          DCHECK(pic->ref && pic->long_term);
          // Ok to cast, max_long_term_frame_idx is much smaller than 16bit.
          if (pic->long_term_frame_idx >
              static_cast<int>(max_long_term_frame_idx_))
            pic->ref = false;
        }
        break;
      }

      case 5:
        // Unmark all reference pictures.
        dpb_.MarkAllUnusedForRef();
        max_long_term_frame_idx_ = -1;
        curr_pic_->mem_mgmt_5 = true;
        break;

      case 6: {
        // Replace long term reference pictures with current picture.
        // First unmark if any existing with this long_term_frame_idx...
        H264Picture::Vector long_terms;
        dpb_.GetLongTermRefPicsAppending(&long_terms);
        for (size_t i = 0; i < long_terms.size(); ++i) {
          scoped_refptr<H264Picture>& pic = long_terms[i];
          DCHECK(pic->ref && pic->long_term);
          // Ok to cast, long_term_frame_idx is much smaller than 16bit.
          if (pic->long_term_frame_idx ==
              static_cast<int>(ref_pic_marking->long_term_frame_idx))
            pic->ref = false;
        }

        // and mark the current one instead.
        curr_pic_->ref = true;
        curr_pic_->long_term = true;
        curr_pic_->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        break;
      }

      default:
        // Would indicate a bug in parser.
        NOTREACHED();
    }
  }

  return true;
}

// This method ensures that DPB does not overflow, either by removing
// reference pictures as specified in the stream, or using a sliding window
// procedure to remove the oldest one.
// It also performs marking and unmarking pictures as reference.
// See spac 8.2.5.1.
void H264Decoder::ReferencePictureMarking() {
  if (curr_pic_->idr) {
    // If current picture is an IDR, all reference pictures are unmarked.
    dpb_.MarkAllUnusedForRef();

    if (curr_pic_->long_term_reference_flag) {
      curr_pic_->long_term = true;
      curr_pic_->long_term_frame_idx = 0;
      max_long_term_frame_idx_ = 0;
    } else {
      curr_pic_->long_term = false;
      max_long_term_frame_idx_ = -1;
    }
  } else {
    if (!curr_pic_->adaptive_ref_pic_marking_mode_flag) {
      // If non-IDR, and the stream does not indicate what we should do to
      // ensure DPB doesn't overflow, discard oldest picture.
      // See spec 8.2.5.3.
      if (curr_pic_->field == H264Picture::FIELD_NONE) {
        DCHECK_LE(
            dpb_.CountRefPics(),
            std::max<int>(parser_.GetSPS(curr_sps_id_)->max_num_ref_frames, 1));
        if (dpb_.CountRefPics() ==
            std::max<int>(parser_.GetSPS(curr_sps_id_)->max_num_ref_frames,
                          1)) {
          // Max number of reference pics reached,
          // need to remove one of the short term ones.
          // Find smallest frame_num_wrap short reference picture and mark
          // it as unused.
          scoped_refptr<H264Picture> to_unmark =
              dpb_.GetLowestFrameNumWrapShortRefPic();
          if (to_unmark == NULL) {
            DVLOG(1) << "Couldn't find a short ref picture to unmark";
            return;
          }
          to_unmark->ref = false;
        }
      } else {
        // Shouldn't get here.
        DVLOG(1) << "Interlaced video not supported.";
      }
    } else {
      // Stream has instructions how to discard pictures from DPB and how
      // to mark/unmark existing reference pictures. Do it.
      // Spec 8.2.5.4.
      if (curr_pic_->field == H264Picture::FIELD_NONE) {
        HandleMemoryManagementOps();
      } else {
        // Shouldn't get here.
        DVLOG(1) << "Interlaced video not supported.";
      }
    }
  }
}

bool H264Decoder::FinishPicture() {
  DCHECK(curr_pic_.get());

  // Finish processing previous picture.
  // Start by storing previous reference picture data for later use,
  // if picture being finished is a reference picture.
  if (curr_pic_->ref) {
    ReferencePictureMarking();
    prev_ref_has_memmgmnt5_ = curr_pic_->mem_mgmt_5;
    prev_ref_top_field_order_cnt_ = curr_pic_->top_field_order_cnt;
    prev_ref_pic_order_cnt_msb_ = curr_pic_->pic_order_cnt_msb;
    prev_ref_pic_order_cnt_lsb_ = curr_pic_->pic_order_cnt_lsb;
    prev_ref_field_ = curr_pic_->field;
  }
  prev_has_memmgmnt5_ = curr_pic_->mem_mgmt_5;
  prev_frame_num_offset_ = curr_pic_->frame_num_offset;

  // Remove unused (for reference or later output) pictures from DPB, marking
  // them as such.
  dpb_.DeleteUnused();

  DVLOG(4) << "Finishing picture, entries in DPB: " << dpb_.size();

  // Whatever happens below, curr_pic_ will stop managing the pointer to the
  // picture after this. The ownership will either be transferred to DPB, if
  // the image is still needed (for output and/or reference), or the memory
  // will be released if we manage to output it here without having to store
  // it for future reference.
  scoped_refptr<H264Picture> pic = curr_pic_;
  curr_pic_ = nullptr;

  // Get all pictures that haven't been outputted yet.
  H264Picture::Vector not_outputted;
  dpb_.GetNotOutputtedPicsAppending(&not_outputted);
  // Include the one we've just decoded.
  not_outputted.push_back(pic);

  // Sort in output order.
  std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

  // Try to output as many pictures as we can. A picture can be output,
  // if the number of decoded and not yet outputted pictures that would remain
  // in DPB afterwards would at least be equal to max_num_reorder_frames.
  // If the outputted picture is not a reference picture, it doesn't have
  // to remain in the DPB and can be removed.
  H264Picture::Vector::iterator output_candidate = not_outputted.begin();
  size_t num_remaining = not_outputted.size();
  while (num_remaining > max_num_reorder_frames_) {
    int poc = (*output_candidate)->pic_order_cnt;
    DCHECK_GE(poc, last_output_poc_);
    OutputPic(*output_candidate);

    if (!(*output_candidate)->ref) {
      // Current picture hasn't been inserted into DPB yet, so don't remove it
      // if we managed to output it immediately.
      if ((*output_candidate)->pic_order_cnt != pic->pic_order_cnt)
        dpb_.DeleteByPOC(poc);
    }

    ++output_candidate;
    --num_remaining;
  }

  // If we haven't managed to output the picture that we just decoded, or if
  // it's a reference picture, we have to store it in DPB.
  if (!pic->outputted || pic->ref) {
    if (dpb_.IsFull()) {
      // If we haven't managed to output anything to free up space in DPB
      // to store this picture, it's an error in the stream.
      DVLOG(1) << "Could not free up space in DPB!";
      return false;
    }

    dpb_.StorePic(pic);
  }

  return true;
}

static int LevelToMaxDpbMbs(int level) {
  // See table A-1 in spec.
  switch (level) {
    case 10: return 396;
    case 11: return 900;
    case 12: //  fallthrough
    case 13: //  fallthrough
    case 20: return 2376;
    case 21: return 4752;
    case 22: //  fallthrough
    case 30: return 8100;
    case 31: return 18000;
    case 32: return 20480;
    case 40: //  fallthrough
    case 41: return 32768;
    case 42: return 34816;
    case 50: return 110400;
    case 51: //  fallthrough
    case 52: return 184320;
    default:
      DVLOG(1) << "Invalid codec level (" << level << ")";
      return 0;
  }
}

bool H264Decoder::UpdateMaxNumReorderFrames(const media::H264SPS* sps) {
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    max_num_reorder_frames_ =
        base::checked_cast<size_t>(sps->max_num_reorder_frames);
    if (max_num_reorder_frames_ > dpb_.max_num_pics()) {
      DVLOG(1)
          << "max_num_reorder_frames present, but larger than MaxDpbFrames ("
          << max_num_reorder_frames_ << " > " << dpb_.max_num_pics() << ")";
      max_num_reorder_frames_ = 0;
      return false;
    }
    return true;
  }

  // max_num_reorder_frames not present, infer from profile/constraints
  // (see VUI semantics in spec).
  if (sps->constraint_set3_flag) {
    switch (sps->profile_idc) {
      case 44:
      case 86:
      case 100:
      case 110:
      case 122:
      case 244:
        max_num_reorder_frames_ = 0;
        break;
      default:
        max_num_reorder_frames_ = dpb_.max_num_pics();
        break;
    }
  } else {
    max_num_reorder_frames_ = dpb_.max_num_pics();
  }

  return true;
}

bool H264Decoder::ProcessSPS(int sps_id, bool* need_new_buffers) {
  const media::H264SPS* sps = parser_.GetSPS(sps_id);
  DCHECK(sps);
  DVLOG(4) << "Processing SPS";

  *need_new_buffers = false;

  if (sps->frame_mbs_only_flag == 0) {
    DVLOG(1) << "frame_mbs_only_flag != 1 not supported";
    return false;
  }

  if (sps->gaps_in_frame_num_value_allowed_flag) {
    DVLOG(1) << "Gaps in frame numbers not supported";
    return false;
  }

  curr_sps_id_ = sps->seq_parameter_set_id;

  // Calculate picture height/width in macroblocks and pixels
  // (spec 7.4.2.1.1, 7.4.3).
  int width_mb = sps->pic_width_in_mbs_minus1 + 1;
  int height_mb = (2 - sps->frame_mbs_only_flag) *
                  (sps->pic_height_in_map_units_minus1 + 1);

  gfx::Size new_pic_size(16 * width_mb, 16 * height_mb);
  if (new_pic_size.IsEmpty()) {
    DVLOG(1) << "Invalid picture size: " << new_pic_size.ToString();
    return false;
  }

  if (!pic_size_.IsEmpty() && new_pic_size == pic_size_) {
    // Already have surfaces and this SPS keeps the same resolution,
    // no need to request a new set.
    return true;
  }

  pic_size_ = new_pic_size;
  DVLOG(1) << "New picture size: " << pic_size_.ToString();

  max_pic_order_cnt_lsb_ = 1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  max_frame_num_ = 1 << (sps->log2_max_frame_num_minus4 + 4);

  int level = sps->level_idc;
  int max_dpb_mbs = LevelToMaxDpbMbs(level);
  if (max_dpb_mbs == 0)
    return false;

  size_t max_dpb_size = std::min(max_dpb_mbs / (width_mb * height_mb),
                                 static_cast<int>(H264DPB::kDPBMaxSize));
  DVLOG(1) << "Codec level: " << level << ", DPB size: " << max_dpb_size;
  if (max_dpb_size == 0) {
    DVLOG(1) << "Invalid DPB Size";
    return false;
  }

  dpb_.set_max_num_pics(max_dpb_size);

  if (!UpdateMaxNumReorderFrames(sps))
    return false;
  DVLOG(1) << "max_num_reorder_frames: " << max_num_reorder_frames_;

  *need_new_buffers = true;
  return true;
}

bool H264Decoder::ProcessPPS(int pps_id) {
  const media::H264PPS* pps = parser_.GetPPS(pps_id);
  DCHECK(pps);

  curr_pps_id_ = pps->pic_parameter_set_id;

  return true;
}

bool H264Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_ != NULL) {
    if (!DecodePicture())
      return false;
    return FinishPicture();
  }

  return true;
}

bool H264Decoder::PreprocessSlice(media::H264SliceHeader* slice_hdr) {
  prev_frame_num_ = frame_num_;
  frame_num_ = slice_hdr->frame_num;

  if (prev_frame_num_ > 0 && prev_frame_num_ < frame_num_ - 1) {
    DVLOG(1) << "Gap in frame_num!";
    return false;
  }

  if (slice_hdr->field_pic_flag == 0)
    max_pic_num_ = max_frame_num_;
  else
    max_pic_num_ = 2 * max_frame_num_;

  // TODO posciak: switch to new picture detection per 7.4.1.2.4.
  if (curr_pic_ != NULL && slice_hdr->first_mb_in_slice != 0) {
    // More slice data of the current picture.
    return true;
  } else {
    // A new frame, so first finish the previous one before processing it...
    if (!FinishPrevFrameIfPresent())
      return false;
  }

  // If the new frame is an IDR, output what's left to output and clear DPB
  if (slice_hdr->idr_pic_flag) {
    // (unless we are explicitly instructed not to do so).
    if (!slice_hdr->no_output_of_prior_pics_flag) {
      // Output DPB contents.
      if (!Flush())
        return false;
    }
    dpb_.Clear();
    last_output_poc_ = std::numeric_limits<int>::min();
  }

  return true;
}

bool H264Decoder::ProcessSlice(media::H264SliceHeader* slice_hdr) {
  DCHECK(curr_pic_.get());
  H264Picture::Vector ref_pic_list0, ref_pic_list1;

  if (!ModifyReferencePicLists(slice_hdr, &ref_pic_list0, &ref_pic_list1))
    return false;

  const media::H264PPS* pps = parser_.GetPPS(slice_hdr->pic_parameter_set_id);
  DCHECK(pps);

  if (!accelerator_->SubmitSlice(pps, slice_hdr, ref_pic_list0, ref_pic_list1,
                                 curr_pic_.get(), slice_hdr->nalu_data,
                                 slice_hdr->nalu_size))
    return false;

  curr_slice_hdr_.reset();
  return true;
}

#define SET_ERROR_AND_RETURN()         \
  do {                                 \
    DVLOG(1) << "Error during decode"; \
    state_ = kError;                   \
    return H264Decoder::kDecodeError;  \
  } while (0)

void H264Decoder::SetStream(const uint8_t* ptr, size_t size) {
  DCHECK(ptr);
  DCHECK(size);

  DVLOG(4) << "New input stream at: " << (void*)ptr << " size: " << size;
  parser_.SetStream(ptr, size);
}

H264Decoder::DecodeResult H264Decoder::Decode() {
  DCHECK_NE(state_, kError);

  while (1) {
    media::H264Parser::Result par_res;

    if (!curr_nalu_) {
      curr_nalu_.reset(new media::H264NALU());
      par_res = parser_.AdvanceToNextNALU(curr_nalu_.get());
      if (par_res == media::H264Parser::kEOStream)
        return kRanOutOfStreamData;
      else if (par_res != media::H264Parser::kOk)
        SET_ERROR_AND_RETURN();
    }

    DVLOG(4) << "NALU found: " << static_cast<int>(curr_nalu_->nal_unit_type);

    switch (curr_nalu_->nal_unit_type) {
      case media::H264NALU::kNonIDRSlice:
        // We can't resume from a non-IDR slice.
        if (state_ != kDecoding)
          break;
        // else fallthrough
      case media::H264NALU::kIDRSlice: {
        // TODO(posciak): the IDR may require an SPS that we don't have
        // available. For now we'd fail if that happens, but ideally we'd like
        // to keep going until the next SPS in the stream.
        if (state_ == kNeedStreamMetadata) {
          // We need an SPS, skip this IDR and keep looking.
          break;
        }

        // If after reset, we should be able to recover from an IDR.
        if (!curr_slice_hdr_) {
          curr_slice_hdr_.reset(new media::H264SliceHeader());
          par_res =
              parser_.ParseSliceHeader(*curr_nalu_, curr_slice_hdr_.get());
          if (par_res != media::H264Parser::kOk)
            SET_ERROR_AND_RETURN();

          if (!PreprocessSlice(curr_slice_hdr_.get()))
            SET_ERROR_AND_RETURN();
        }

        if (!curr_pic_) {
          // New picture/finished previous one, try to start a new one
          // or tell the client we need more surfaces.
          curr_pic_ = accelerator_->CreateH264Picture();
          if (!curr_pic_)
            return kRanOutOfSurfaces;

          if (!StartNewFrame(curr_slice_hdr_.get()))
            SET_ERROR_AND_RETURN();
        }

        if (!ProcessSlice(curr_slice_hdr_.get()))
          SET_ERROR_AND_RETURN();

        state_ = kDecoding;
        break;
      }

      case media::H264NALU::kSPS: {
        int sps_id;

        if (!FinishPrevFrameIfPresent())
          SET_ERROR_AND_RETURN();

        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != media::H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        bool need_new_buffers = false;
        if (!ProcessSPS(sps_id, &need_new_buffers))
          SET_ERROR_AND_RETURN();

        state_ = kDecoding;

        if (need_new_buffers) {
          if (!Flush())
            return kDecodeError;

          curr_pic_ = nullptr;
          curr_nalu_ = nullptr;
          ref_pic_list_p0_.clear();
          ref_pic_list_b0_.clear();
          ref_pic_list_b1_.clear();

          return kAllocateNewSurfaces;
        }
        break;
      }

      case media::H264NALU::kPPS: {
        if (state_ != kDecoding)
          break;

        int pps_id;

        if (!FinishPrevFrameIfPresent())
          SET_ERROR_AND_RETURN();

        par_res = parser_.ParsePPS(&pps_id);
        if (par_res != media::H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessPPS(pps_id))
          SET_ERROR_AND_RETURN();
        break;
      }

      default:
        DVLOG(4) << "Skipping NALU type: " << curr_nalu_->nal_unit_type;
        break;
    }

    DVLOG(4) << "Dropping nalu";
    curr_nalu_.reset();
  }
}

gfx::Size H264Decoder::GetPicSize() const {
  return pic_size_;
}

size_t H264Decoder::GetRequiredNumOfPictures() const {
  return dpb_.max_num_pics() + kPicsInPipeline;
}

}  // namespace content
