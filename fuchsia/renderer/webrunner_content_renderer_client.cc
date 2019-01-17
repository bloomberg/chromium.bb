// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/renderer/webrunner_content_renderer_client.h"

#include "base/macros.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia/renderer/on_load_script_injector.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace webrunner {

WebRunnerContentRendererClient::WebRunnerContentRendererClient() = default;

WebRunnerContentRendererClient::~WebRunnerContentRendererClient() = default;

void WebRunnerContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add WebRunner services to the new RenderFrame.
  // The objects' lifetimes are bound to the RenderFrame's lifetime.
  new OnLoadScriptInjector(render_frame);
}

}  // namespace webrunner
