// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/content/common/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"
#include "components/dom_distiller/content/renderer/distiller_page_notifier_service_impl.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace dom_distiller {

// DistillerJsRenderFrame observer waits for a page to be loaded and then
// tried to connect to a mojo service hosted in the browser process. The
// service will tell this render process if the current page is a distiller
// page.
class DistillerJsRenderFrameObserver : public content::RenderFrameObserver {
 public:
  DistillerJsRenderFrameObserver(content::RenderFrame* render_frame,
                                 const int distiller_isolated_world_id);
  ~DistillerJsRenderFrameObserver() override;

  // RenderFrameObserver implementation.
  void DidStartProvisionalLoad() override;
  void DidFinishLoad() override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int extension_group,
                              int world_id) override;

  // Add the mojo service to a RenderFrame's service registry.
  void RegisterMojoService();
  // Flag the current page as a distiller page.
  void SetIsDistillerPage();

 private:
  void CreateDistillerPageNotifierService(
      mojo::InterfaceRequest<DistillerPageNotifierService> request);

  // The isolated world that the distiller object should be written to.
  int distiller_isolated_world_id_;

  // Track if the current page is distilled. This is needed for testing.
  bool is_distiller_page_;

  //mojo::StrongBinding<DistillerPageNotifierService> binding_;

  // Handle to "distiller" JavaScript object functionality.
  scoped_ptr<DistillerNativeJavaScript> native_javascript_handle_;
  base::WeakPtrFactory<DistillerJsRenderFrameObserver> weak_factory_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_JS_RENDER_FRAME_OBSERVER_H_
