// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"

namespace content {

RenderFrameMetadataProviderImpl::RenderFrameMetadataProviderImpl(
    mojom::RenderFrameMetadataObserverClientRequest client_request,
    mojom::RenderFrameMetadataObserverPtr observer)
    : render_frame_metadata_observer_client_binding_(this),
      render_frame_metadata_observer_ptr_(std::move(observer)) {
  render_frame_metadata_observer_client_binding_.Bind(
      std::move(client_request));
}

RenderFrameMetadataProviderImpl::~RenderFrameMetadataProviderImpl() = default;

void RenderFrameMetadataProviderImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RenderFrameMetadataProviderImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RenderFrameMetadataProviderImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  render_frame_metadata_observer_ptr_->ReportAllFrameSubmissionsForTesting(
      enabled);
}

const cc::RenderFrameMetadata&
RenderFrameMetadataProviderImpl::LastRenderFrameMetadata() const {
  return last_render_frame_metadata_;
}

void RenderFrameMetadataProviderImpl::OnRenderFrameMetadataChanged(
    const cc::RenderFrameMetadata& metadata) {
  last_render_frame_metadata_ = metadata;
  for (Observer& observer : observers_)
    observer.OnRenderFrameMetadataChanged();
}

void RenderFrameMetadataProviderImpl::OnFrameSubmissionForTesting() {
  for (Observer& observer : observers_)
    observer.OnRenderFrameSubmission();
}

}  // namespace content
