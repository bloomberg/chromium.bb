// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COPY_OUTPUT_REQUEST_H_
#define CC_OUTPUT_COPY_OUTPUT_REQUEST_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"

class SkBitmap;

namespace cc {

class CC_EXPORT CopyOutputRequest {
 public:
  typedef base::Callback<void(scoped_ptr<SkBitmap>)> CopyAsBitmapCallback;

  static scoped_ptr<CopyOutputRequest> CreateBitmapRequest(
      const CopyAsBitmapCallback& bitmap_callback) {
    return make_scoped_ptr(new CopyOutputRequest(bitmap_callback));
  }

  ~CopyOutputRequest();

  bool IsEmpty() const { return !HasBitmapRequest(); }
  bool HasBitmapRequest() const { return !bitmap_callback_.is_null(); }

  void SendEmptyResult();
  void SendBitmapResult(scoped_ptr<SkBitmap> bitmap);

  bool Equals(const CopyOutputRequest& other) const {
    return bitmap_callback_.Equals(other.bitmap_callback_);
  }

 private:
  explicit CopyOutputRequest(const CopyAsBitmapCallback& callback);

  CopyAsBitmapCallback bitmap_callback_;
};

}  // namespace cc

#endif  // CC_OUTPUT_COPY_OUTPUT_REQUEST_H_
