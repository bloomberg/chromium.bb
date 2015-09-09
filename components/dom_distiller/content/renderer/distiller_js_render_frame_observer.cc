// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"

#include "base/bind.h"
#include "components/dom_distiller/content/common/distiller_page_notifier_service.mojom.h"
#include "components/dom_distiller/content/renderer/distiller_page_notifier_service_impl.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "v8/include/v8.h"

namespace dom_distiller {

DistillerJsRenderFrameObserver::DistillerJsRenderFrameObserver(
    content::RenderFrame* render_frame,
    const int distiller_isolated_world_id)
    : RenderFrameObserver(render_frame),
      distiller_isolated_world_id_(distiller_isolated_world_id),
      is_distiller_page_(false),
      weak_factory_(this) {}

DistillerJsRenderFrameObserver::~DistillerJsRenderFrameObserver() {}

void DistillerJsRenderFrameObserver::DidStartProvisionalLoad() {
  RegisterMojoService();
}

void DistillerJsRenderFrameObserver::DidFinishLoad() {
  // If no message about the distilled page was received at this point, there
  // will not be one; remove the DistillerPageNotifierService from the registry.
  render_frame()
      ->GetServiceRegistry()
      ->RemoveService<DistillerPageNotifierService>();
}

void DistillerJsRenderFrameObserver::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int extension_group,
    int world_id) {
  if (world_id != distiller_isolated_world_id_ || !is_distiller_page_) {
    return;
  }

  native_javascript_handle_.reset(
      new DistillerNativeJavaScript(render_frame()));
  native_javascript_handle_->AddJavaScriptObjectToFrame(context);
}

void DistillerJsRenderFrameObserver::RegisterMojoService() {
  render_frame()->GetServiceRegistry()->AddService(base::Bind(
      &DistillerJsRenderFrameObserver::CreateDistillerPageNotifierService,
      weak_factory_.GetWeakPtr()));
}

void DistillerJsRenderFrameObserver::CreateDistillerPageNotifierService(
    mojo::InterfaceRequest<DistillerPageNotifierService> request) {
  // This is strongly bound to and owned by the pipe.
  new DistillerPageNotifierServiceImpl(this, request.Pass());
}

void DistillerJsRenderFrameObserver::SetIsDistillerPage() {
  is_distiller_page_ = true;
}

}  // namespace dom_distiller
