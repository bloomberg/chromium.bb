// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_view_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace client {

BlimpContentsViewImpl::BlimpContentsViewImpl(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer)
    : blimp_contents_(blimp_contents),
      contents_layer_(std::move(contents_layer)),
      weak_ptr_factory_(this) {
  DCHECK(blimp_contents_);
  DCHECK(contents_layer_);
}

BlimpContentsViewImpl::~BlimpContentsViewImpl() = default;

scoped_refptr<cc::Layer> BlimpContentsViewImpl::GetLayer() {
  return contents_layer_;
}

void BlimpContentsViewImpl::SetSizeAndScale(const gfx::Size& size,
                                            float device_scale_factor) {
  blimp_contents_->SetSizeAndScale(size, device_scale_factor);
}

bool BlimpContentsViewImpl::OnTouchEvent(const ui::MotionEvent& motion_event) {
  return blimp_contents_->document_manager()->OnTouchEvent(motion_event);
}

void BlimpContentsViewImpl::CopyFromCompositingSurface(
    const ReadbackRequestCallback& callback) {
  blimp_contents_->document_manager()->NotifyWhenDonePendingCommits(
      base::Bind(&BlimpContentsViewImpl::StartReadbackRequest,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BlimpContentsViewImpl::StartReadbackRequest(
    const ReadbackRequestCallback& callback) {
  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&BlimpContentsViewImpl::OnReadbackComplete,
                     weak_ptr_factory_.GetWeakPtr(), callback));
  contents_layer_->RequestCopyOfOutput(std::move(request));
}

void BlimpContentsViewImpl::OnReadbackComplete(
    const ReadbackRequestCallback& callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
  callback.Run(result->TakeBitmap());
}

}  // namespace client
}  // namespace blimp
