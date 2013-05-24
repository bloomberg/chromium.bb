// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_request.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

CopyOutputRequest::CopyOutputRequest(const CopyAsBitmapCallback& callback)
    : bitmap_callback_(callback) {}

CopyOutputRequest::~CopyOutputRequest() {
  SendEmptyResult();
}

void CopyOutputRequest::SendEmptyResult() {
  if (!bitmap_callback_.is_null())
    base::ResetAndReturn(&bitmap_callback_).Run(scoped_ptr<SkBitmap>());
}

void CopyOutputRequest::SendBitmapResult(scoped_ptr<SkBitmap> bitmap) {
  DCHECK(HasBitmapRequest());
  base::ResetAndReturn(&bitmap_callback_).Run(bitmap.Pass());
}

}  // namespace cc
