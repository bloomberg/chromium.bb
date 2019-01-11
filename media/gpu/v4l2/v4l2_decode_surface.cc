// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include <linux/videodev2.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/macros.h"

namespace media {

V4L2DecodeSurface::V4L2DecodeSurface(int input_record,
                                     int output_record,
                                     ReleaseCB release_cb)
    : input_record_(input_record),
      output_record_(output_record),
      decoded_(false),
      release_cb_(std::move(release_cb)) {}

V4L2DecodeSurface::~V4L2DecodeSurface() {
  DVLOGF(5) << "Releasing output record id=" << output_record_;
  if (release_cb_)
    std::move(release_cb_).Run(output_record_);
}

void V4L2DecodeSurface::SetDecoded() {
  DCHECK(!decoded_);
  decoded_ = true;

  // We can now drop references to all reference surfaces for this surface
  // as we are done with decoding.
  reference_surfaces_.clear();

  // And finally execute and drop the decode done callback, if set.
  if (done_cb_)
    std::move(done_cb_).Run();
}

void V4L2DecodeSurface::SetVisibleRect(const gfx::Rect& visible_rect) {
  visible_rect_ = visible_rect;
}

void V4L2DecodeSurface::SetReferenceSurfaces(
    std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces) {
  DCHECK(reference_surfaces_.empty());
  reference_surfaces_ = std::move(ref_surfaces);
}

void V4L2DecodeSurface::SetDecodeDoneCallback(base::OnceClosure done_cb) {
  DCHECK(!done_cb_);
  done_cb_ = std::move(done_cb);
}

std::string V4L2DecodeSurface::ToString() const {
  std::string out;
  base::StringAppendF(&out, "Buffer %d -> %d. ", input_record_, output_record_);
  base::StringAppendF(&out, "Reference surfaces:");
  for (const auto& ref : reference_surfaces_) {
    DCHECK_NE(ref->output_record(), output_record_);
    base::StringAppendF(&out, " %d", ref->output_record());
  }
  return out;
}

void V4L2ConfigStoreDecodeSurface::PrepareSetCtrls(
    struct v4l2_ext_controls* ctrls) const {
  DCHECK_NE(ctrls, nullptr);
  DCHECK_GT(config_store_, 0u);

  ctrls->config_store = config_store_;
}

void V4L2ConfigStoreDecodeSurface::PrepareQueueBuffer(
    struct v4l2_buffer* buffer) const {
  DCHECK_NE(buffer, nullptr);
  DCHECK_GT(config_store_, 0u);

  buffer->config_store = config_store_;
}

uint64_t V4L2ConfigStoreDecodeSurface::GetReferenceID() const {
  // Control store uses the output buffer ID as reference.
  return output_record();
}

bool V4L2ConfigStoreDecodeSurface::Submit() const {
  // There is nothing extra to submit when using the config store
  return true;
}

}  // namespace media
