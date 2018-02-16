// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_metadata_observer_impl.h"

namespace content {

RenderFrameMetadataObserverImpl::RenderFrameMetadataObserverImpl(
    mojom::RenderFrameMetadataObserverRequest request,
    mojom::RenderFrameMetadataObserverClientPtrInfo client_info)
    : request_(std::move(request)),
      client_info_(std::move(client_info)),
      render_frame_metadata_observer_binding_(this) {}

RenderFrameMetadataObserverImpl::~RenderFrameMetadataObserverImpl() {}

void RenderFrameMetadataObserverImpl::BindToCurrentThread() {
  DCHECK(request_.is_pending());
  render_frame_metadata_observer_binding_.Bind(std::move(request_));
  render_frame_metadata_observer_client_.Bind(std::move(client_info_));
}

void RenderFrameMetadataObserverImpl::OnRenderFrameSubmission(
    const cc::RenderFrameMetadata& metadata) {
  bool metadata_changed = last_render_frame_metadata_ != metadata;
  last_render_frame_metadata_ = metadata;

  if (!render_frame_metadata_observer_client_)
    return;

  if (report_all_frame_submissions_for_testing_enabled_)
    render_frame_metadata_observer_client_->OnFrameSubmissionForTesting();

  if (metadata_changed) {
    render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
        metadata);
  }
}

void RenderFrameMetadataObserverImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  report_all_frame_submissions_for_testing_enabled_ = enabled;
}

}  // namespace content
