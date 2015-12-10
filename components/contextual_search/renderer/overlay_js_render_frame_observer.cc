// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/renderer/overlay_js_render_frame_observer.h"

#include "base/bind.h"
#include "components/contextual_search/renderer/contextual_search_wrapper.h"
#include "components/contextual_search/renderer/overlay_page_notifier_service_impl.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "v8/include/v8.h"

namespace contextual_search {

OverlayJsRenderFrameObserver::OverlayJsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      is_contextual_search_overlay_(false),
      weak_factory_(this) {}

OverlayJsRenderFrameObserver::~OverlayJsRenderFrameObserver() {}

void OverlayJsRenderFrameObserver::DidStartProvisionalLoad() {
  RegisterMojoService();
}

void OverlayJsRenderFrameObserver::RegisterMojoService() {
  render_frame()->GetServiceRegistry()->AddService(base::Bind(
      &OverlayJsRenderFrameObserver::CreateOverlayPageNotifierService,
      weak_factory_.GetWeakPtr()));
}

void OverlayJsRenderFrameObserver::CreateOverlayPageNotifierService(
    mojo::InterfaceRequest<OverlayPageNotifierService> request) {
  // This is strongly bound to and owned by the pipe.
  new OverlayPageNotifierServiceImpl(this, request.Pass());
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
  render_frame()
      ->GetServiceRegistry()
      ->RemoveService<OverlayPageNotifierService>();
}

}  // namespace contextual_search
