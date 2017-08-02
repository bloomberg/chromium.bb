// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/renderer/overlay_js_render_frame_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/contextual_search/renderer/contextual_search_wrapper.h"
#include "components/contextual_search/renderer/overlay_page_notifier_service_impl.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "v8/include/v8.h"

namespace contextual_search {

OverlayJsRenderFrameObserver::OverlayJsRenderFrameObserver(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : RenderFrameObserver(render_frame),
      is_contextual_search_overlay_(false),
      weak_factory_(this) {
  registry->AddInterface(base::Bind(
      &OverlayJsRenderFrameObserver::CreateOverlayPageNotifierService,
      weak_factory_.GetWeakPtr()));
}

OverlayJsRenderFrameObserver::~OverlayJsRenderFrameObserver() {}

void OverlayJsRenderFrameObserver::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader) {
  can_bind_requests_ = true;
}

void OverlayJsRenderFrameObserver::CreateOverlayPageNotifierService(
    mojom::OverlayPageNotifierServiceRequest request) {
  if (!can_bind_requests_)
    return;
  mojo::MakeStrongBinding(
      base::MakeUnique<OverlayPageNotifierServiceImpl>(
          weak_factory_.GetWeakPtr()),
      std::move(request));
}

void OverlayJsRenderFrameObserver::SetIsContextualSearchOverlay() {
  is_contextual_search_overlay_ = true;
}

void OverlayJsRenderFrameObserver::DidClearWindowObject() {
  if (is_contextual_search_overlay_) {
    contextual_search::ContextualSearchWrapper::Install(render_frame());
  }
}

void OverlayJsRenderFrameObserver::DidFinishLoad() {
  // If no message about the Contextual Search overlay was received at this
  // point, there will not be one; remove the OverlayPageNotifierService
  // from the registry.
  DestroyOverlayPageNotifierService();
}

void OverlayJsRenderFrameObserver::DestroyOverlayPageNotifierService() {
  can_bind_requests_ = false;
}

void OverlayJsRenderFrameObserver::OnDestruct() {
  DestroyOverlayPageNotifierService();
  delete this;
}

}  // namespace contextual_search
