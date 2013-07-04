// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_request.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/texture_mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

CopyOutputRequest::CopyOutputRequest() {}

CopyOutputRequest::CopyOutputRequest(
    bool force_bitmap_result,
    const CopyOutputRequestCallback& result_callback)
    : force_bitmap_result_(force_bitmap_result),
      has_area_(false),
      result_callback_(result_callback) {
}

CopyOutputRequest::~CopyOutputRequest() {
  if (!result_callback_.is_null())
    SendResult(CopyOutputResult::CreateEmptyResult().Pass());
}

void CopyOutputRequest::SendResult(scoped_ptr<CopyOutputResult> result) {
  base::ResetAndReturn(&result_callback_).Run(result.Pass());
}

void CopyOutputRequest::SendEmptyResult() {
  SendResult(CopyOutputResult::CreateEmptyResult().Pass());
}

void CopyOutputRequest::SendBitmapResult(scoped_ptr<SkBitmap> bitmap) {
  SendResult(CopyOutputResult::CreateBitmapResult(bitmap.Pass()).Pass());
}

void CopyOutputRequest::SendTextureResult(gfx::Size size,
                                          scoped_ptr<TextureMailbox> texture) {
  DCHECK(texture->IsTexture());
  SendResult(CopyOutputResult::CreateTextureResult(size,
                                                   texture.Pass()).Pass());
}

}  // namespace cc
