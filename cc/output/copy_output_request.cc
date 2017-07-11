// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_request.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/copy_output_result.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// static
std::unique_ptr<CopyOutputRequest> CopyOutputRequest::CreateRelayRequest(
    const CopyOutputRequest& original_request,
    const CopyOutputRequestCallback& result_callback) {
  std::unique_ptr<CopyOutputRequest> relay = CreateRequest(result_callback);
  relay->force_bitmap_result_ = original_request.force_bitmap_result_;
  relay->area_ = original_request.area_;
  relay->texture_mailbox_ = original_request.texture_mailbox_;
  return relay;
}

CopyOutputRequest::CopyOutputRequest() : force_bitmap_result_(false) {}

CopyOutputRequest::CopyOutputRequest(
    bool force_bitmap_result,
    const CopyOutputRequestCallback& result_callback)
    : force_bitmap_result_(force_bitmap_result),
      result_callback_(result_callback) {
  DCHECK(!result_callback_.is_null());
  TRACE_EVENT_ASYNC_BEGIN0("cc", "CopyOutputRequest", this);
}

CopyOutputRequest::~CopyOutputRequest() {
  if (!result_callback_.is_null())
    SendResult(CopyOutputResult::CreateEmptyResult());
}

void CopyOutputRequest::SendResult(std::unique_ptr<CopyOutputResult> result) {
  bool success = !result->IsEmpty();
  base::ResetAndReturn(&result_callback_).Run(std::move(result));
  TRACE_EVENT_ASYNC_END1("cc", "CopyOutputRequest", this, "success", success);
}

void CopyOutputRequest::SendEmptyResult() {
  SendResult(CopyOutputResult::CreateEmptyResult());
}

void CopyOutputRequest::SendBitmapResult(std::unique_ptr<SkBitmap> bitmap) {
  SendResult(CopyOutputResult::CreateBitmapResult(std::move(bitmap)));
}

void CopyOutputRequest::SendTextureResult(
    const gfx::Size& size,
    const viz::TextureMailbox& texture_mailbox,
    std::unique_ptr<SingleReleaseCallback> release_callback) {
  DCHECK(texture_mailbox.IsTexture());
  SendResult(CopyOutputResult::CreateTextureResult(
      size, texture_mailbox, std::move(release_callback)));
}

void CopyOutputRequest::SetTextureMailbox(
    const viz::TextureMailbox& texture_mailbox) {
  DCHECK(!force_bitmap_result_);
  DCHECK(texture_mailbox.IsTexture());
  texture_mailbox_ = texture_mailbox;
}

}  // namespace cc
