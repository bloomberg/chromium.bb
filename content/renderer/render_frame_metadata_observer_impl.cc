// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_metadata_observer_impl.h"

#include "cc/trees/frame_token_allocator.h"

namespace content {

RenderFrameMetadataObserverImpl::RenderFrameMetadataObserverImpl(
    mojom::RenderFrameMetadataObserverRequest request,
    mojom::RenderFrameMetadataObserverClientPtrInfo client_info)
    : request_(std::move(request)),
      client_info_(std::move(client_info)),
      render_frame_metadata_observer_binding_(this) {}

RenderFrameMetadataObserverImpl::~RenderFrameMetadataObserverImpl() {}

void RenderFrameMetadataObserverImpl::BindToCurrentThread(
    cc::FrameTokenAllocator* frame_token_allocator) {
  DCHECK(request_.is_pending());
  frame_token_allocator_ = frame_token_allocator;
  render_frame_metadata_observer_binding_.Bind(std::move(request_));
  render_frame_metadata_observer_client_.Bind(std::move(client_info_));
}

void RenderFrameMetadataObserverImpl::OnRenderFrameSubmission(
    cc::RenderFrameMetadata metadata) {
  if (!render_frame_metadata_observer_client_)
    return;

  // By default only report metadata changes for fields which have a low
  // frequency of change. However if there are changes in high frequency fields
  // these can be reported while testing is enabled.
  bool send_metadata = false;
  if (report_all_frame_submissions_for_testing_enabled_) {
    render_frame_metadata_observer_client_->OnFrameSubmissionForTesting(
        frame_token_allocator_->GetOrAllocateFrameToken());
    send_metadata = last_render_frame_metadata_ != metadata;
  } else {
    // Sending |root_scroll_offset| outside of tests would leave the browser
    // process with out of date information. It is an optional parameter which
    // we clear here.
    metadata.root_scroll_offset = base::nullopt;
    send_metadata = cc::RenderFrameMetadata::HasAlwaysUpdateMetadataChanged(
        last_render_frame_metadata_, metadata);
  }

  if (send_metadata) {
    render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
        frame_token_allocator_->GetOrAllocateFrameToken(), metadata);
  }

  last_render_frame_metadata_ = metadata;
}

void RenderFrameMetadataObserverImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  report_all_frame_submissions_for_testing_enabled_ = enabled;
}

}  // namespace content
