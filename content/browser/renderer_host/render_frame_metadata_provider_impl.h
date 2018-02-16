// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

// Observes RenderFrameMetadata associated with the submission of a frame for a
// given RenderWidgetHost. The renderer will notify this when sumitting a
// CompositorFrame.
//
// When ReportAllFrameSubmissionsForTesting(true) is called, this will be
// notified of all frame submissions.
//
// All RenderFrameMetadataProvider::Observer will be notified.
class RenderFrameMetadataProviderImpl
    : public RenderFrameMetadataProvider,
      public mojom::RenderFrameMetadataObserverClient {
 public:
  RenderFrameMetadataProviderImpl(
      mojom::RenderFrameMetadataObserverClientRequest client_request,
      mojom::RenderFrameMetadataObserverPtr observer);
  ~RenderFrameMetadataProviderImpl() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Notifies the renderer to begin sending a notification on all frame
  // submissions.
  void ReportAllFrameSubmissionsForTesting(bool enabled) override;

  const cc::RenderFrameMetadata& LastRenderFrameMetadata() const override;

 private:
  // mojom::RenderFrameMetadataObserverClient:
  void OnRenderFrameMetadataChanged(
      const cc::RenderFrameMetadata& metadata) override;
  void OnFrameSubmissionForTesting() override;

  base::ObserverList<Observer> observers_;

  cc::RenderFrameMetadata last_render_frame_metadata_;

  mojo::Binding<mojom::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_binding_;
  mojom::RenderFrameMetadataObserverPtr render_frame_metadata_observer_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameMetadataProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_METADATA_PROVIDER_IMPL_H_
