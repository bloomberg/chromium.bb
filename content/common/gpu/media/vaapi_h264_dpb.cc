// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/common/gpu/media/vaapi_h264_dpb.h"

namespace content {

VaapiH264DPB::VaapiH264DPB() : max_num_pics_(0) {
}
VaapiH264DPB::~VaapiH264DPB() {
}

void VaapiH264DPB::Clear() {
  pics_.clear();
}

void VaapiH264DPB::set_max_num_pics(size_t max_num_pics) {
  DCHECK_LE(max_num_pics, kDPBMaxSize);
  max_num_pics_ = max_num_pics;
  if (pics_.size() > max_num_pics_)
    pics_.resize(max_num_pics_);
}

void VaapiH264DPB::DeleteByPOC(int poc) {
  for (Pictures::iterator it = pics_.begin(); it != pics_.end(); ++it) {
    if ((*it)->pic_order_cnt == poc) {
      pics_.erase(it);
      return;
    }
  }
  NOTREACHED() << "Missing POC: " << poc;
}

void VaapiH264DPB::DeleteUnused() {
  for (Pictures::iterator it = pics_.begin(); it != pics_.end(); ) {
    if ((*it)->outputted && !(*it)->ref)
      it = pics_.erase(it);
    else
      ++it;
  }
}

void VaapiH264DPB::StorePic(VaapiH264Picture* pic) {
  DCHECK_LT(pics_.size(), max_num_pics_);
  DVLOG(3) << "Adding PicNum: " << pic->pic_num << " ref: " << (int)pic->ref
           << " longterm: " << (int)pic->long_term << " to DPB";
  pics_.push_back(pic);
}

int VaapiH264DPB::CountRefPics() {
  int ret = 0;
  for (size_t i = 0; i < pics_.size(); ++i) {
    if (pics_[i]->ref)
      ++ret;
  }
  return ret;
}

void VaapiH264DPB::MarkAllUnusedForRef() {
  for (size_t i = 0; i < pics_.size(); ++i)
    pics_[i]->ref = false;
}

VaapiH264Picture* VaapiH264DPB::GetShortRefPicByPicNum(int pic_num) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    VaapiH264Picture* pic = pics_[i];
    if (pic->ref && !pic->long_term && pic->pic_num == pic_num)
      return pic;
  }

  DVLOG(1) << "Missing short ref pic num: " << pic_num;
  return NULL;
}

VaapiH264Picture* VaapiH264DPB::GetLongRefPicByLongTermPicNum(int pic_num) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    VaapiH264Picture* pic = pics_[i];
    if (pic->ref && pic->long_term && pic->long_term_pic_num == pic_num)
      return pic;
  }

  DVLOG(1) << "Missing long term pic num: " << pic_num;
  return NULL;
}

VaapiH264Picture* VaapiH264DPB::GetLowestFrameNumWrapShortRefPic() {
  VaapiH264Picture* ret = NULL;
  for (size_t i = 0; i < pics_.size(); ++i) {
    VaapiH264Picture* pic = pics_[i];
    if (pic->ref && !pic->long_term &&
        (!ret || pic->frame_num_wrap < ret->frame_num_wrap))
      ret = pic;
  }
  return ret;
}

void VaapiH264DPB::GetNotOutputtedPicsAppending(
    VaapiH264Picture::PtrVector& out) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    VaapiH264Picture* pic = pics_[i];
    if (!pic->outputted)
      out.push_back(pic);
  }
}

void VaapiH264DPB::GetShortTermRefPicsAppending(
    VaapiH264Picture::PtrVector& out) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    VaapiH264Picture* pic = pics_[i];
    if (pic->ref && !pic->long_term)
      out.push_back(pic);
  }
}

void VaapiH264DPB::GetLongTermRefPicsAppending(
    VaapiH264Picture::PtrVector& out) {
  for (size_t i = 0; i < pics_.size(); ++i) {
    VaapiH264Picture* pic = pics_[i];
    if (pic->ref && pic->long_term)
      out.push_back(pic);
  }
}

}  // namespace content
