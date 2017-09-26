// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace viz {

CopyOutputRequest::CopyOutputRequest(ResultFormat result_format,
                                     CopyOutputRequestCallback result_callback)
    : result_format_(result_format),
      result_callback_(std::move(result_callback)) {
  DCHECK(!result_callback_.is_null());
  TRACE_EVENT_ASYNC_BEGIN0("viz", "CopyOutputRequest", this);
}

CopyOutputRequest::~CopyOutputRequest() {
  if (!result_callback_.is_null()) {
    // Send an empty result to indicate the request was never satisfied.
    SendResult(std::make_unique<CopyOutputResult>(result_format_, gfx::Rect()));
  }
}

void CopyOutputRequest::SendResult(std::unique_ptr<CopyOutputResult> result) {
  TRACE_EVENT_ASYNC_END1("viz", "CopyOutputRequest", this, "success",
                         !result->IsEmpty());
  if (result_task_runner_) {
    result_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback_), std::move(result)));
    result_task_runner_ = nullptr;
  } else {
    std::move(result_callback_).Run(std::move(result));
  }
}

void CopyOutputRequest::SetTextureMailbox(
    const TextureMailbox& texture_mailbox) {
  DCHECK_EQ(result_format_, ResultFormat::RGBA_TEXTURE);
  DCHECK(texture_mailbox.IsTexture());
  texture_mailbox_ = texture_mailbox;
}

// static
std::unique_ptr<CopyOutputRequest> CopyOutputRequest::CreateStubForTesting() {
  return std::make_unique<CopyOutputRequest>(
      ResultFormat::RGBA_BITMAP,
      base::BindOnce([](std::unique_ptr<CopyOutputResult>) {}));
}

}  // namespace viz
