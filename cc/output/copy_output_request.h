// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COPY_OUTPUT_REQUEST_H_
#define CC_OUTPUT_COPY_OUTPUT_REQUEST_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/size.h"

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
    return make_scoped_ptr(new CopyOutputRequest(
        original_request.force_bitmap_result(), result_callback));
  }

  ~CopyOutputRequest();

  bool IsEmpty() const { return result_callback_.is_null(); }
  bool force_bitmap_result() const { return force_bitmap_result_; }

  void SendResult(scoped_ptr<CopyOutputResult> result);
  void SendBitmapResult(scoped_ptr<SkBitmap> bitmap);
  void SendTextureResult(gfx::Size size,
                         scoped_ptr<TextureMailbox> texture_mailbox);

  bool Equals(const CopyOutputRequest& other) const {
    return result_callback_.Equals(other.result_callback_) &&
        force_bitmap_result_ == other.force_bitmap_result_;
  }

 private:
  CopyOutputRequest();
  explicit CopyOutputRequest(bool force_bitmap_result,
                             const CopyOutputRequestCallback& result_callback);

  bool force_bitmap_result_;
  CopyOutputRequestCallback result_callback_;
};

}  // namespace cc

#endif  // CC_OUTPUT_COPY_OUTPUT_REQUEST_H_
