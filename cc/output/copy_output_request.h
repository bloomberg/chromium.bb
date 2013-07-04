// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COPY_OUTPUT_REQUEST_H_
#define CC_OUTPUT_COPY_OUTPUT_REQUEST_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/rect.h"

class SkBitmap;

namespace cc {
class CopyOutputResult;
class TextureMailbox;

class CC_EXPORT CopyOutputRequest {
 public:
  typedef base::Callback<void(scoped_ptr<CopyOutputResult> result)>
      CopyOutputRequestCallback;

  static scoped_ptr<CopyOutputRequest> CreateEmptyRequest() {
    return make_scoped_ptr(new CopyOutputRequest);
  }
  static scoped_ptr<CopyOutputRequest> CreateRequest(
      const CopyOutputRequestCallback& result_callback) {
    return make_scoped_ptr(new CopyOutputRequest(false, result_callback));
  }
  static scoped_ptr<CopyOutputRequest> CreateBitmapRequest(
      const CopyOutputRequestCallback& result_callback) {
    return make_scoped_ptr(new CopyOutputRequest(true, result_callback));
  }
  static scoped_ptr<CopyOutputRequest> CreateRelayRequest(
      const CopyOutputRequest& original_request,
      const CopyOutputRequestCallback& result_callback) {
    scoped_ptr<CopyOutputRequest> relay = CreateRequest(result_callback);
    relay->force_bitmap_result_ = original_request.force_bitmap_result_;
    relay->has_area_ = original_request.has_area_;
    relay->area_ = original_request.area_;
    return relay.Pass();
  }

  ~CopyOutputRequest();

  bool IsEmpty() const { return result_callback_.is_null(); }

  bool force_bitmap_result() const { return force_bitmap_result_; }

  // By default copy requests copy the entire layer's subtree output. If an
  // area is given, then the intersection of this rect (in layer space) with
  // the layer's subtree output will be returned.
  void set_area(gfx::Rect area) {
    has_area_ = true;
    area_ = area;
  }
  bool has_area() const { return has_area_; }
  gfx::Rect area() const { return area_; }

  void SendEmptyResult();
  void SendBitmapResult(scoped_ptr<SkBitmap> bitmap);
  void SendTextureResult(gfx::Size size,
                         scoped_ptr<TextureMailbox> texture_mailbox);

  void SendResult(scoped_ptr<CopyOutputResult> result);

  bool Equals(const CopyOutputRequest& other) const {
    return result_callback_.Equals(other.result_callback_) &&
        force_bitmap_result_ == other.force_bitmap_result_;
  }

 private:
  CopyOutputRequest();
  explicit CopyOutputRequest(bool force_bitmap_result,
                             const CopyOutputRequestCallback& result_callback);

  bool force_bitmap_result_;
  bool has_area_;
  gfx::Rect area_;
  CopyOutputRequestCallback result_callback_;
};

}  // namespace cc

#endif  // CC_OUTPUT_COPY_OUTPUT_REQUEST_H_
