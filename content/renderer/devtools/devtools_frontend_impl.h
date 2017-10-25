// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_FRONTEND_IMPL_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_FRONTEND_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/common/devtools.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/web/WebDevToolsFrontendClient.h"

namespace blink {
class WebDevToolsFrontend;
class WebString;
}  // namespace blink

namespace content {

// Implementation of content.mojom.DevToolsFrontend interface.
class DevToolsFrontendImpl : public RenderFrameObserver,
                             public blink::WebDevToolsFrontendClient,
                             public mojom::DevToolsFrontend {
 public:
  ~DevToolsFrontendImpl() override;

  static void CreateMojoService(
      RenderFrame* render_frame,
      mojom::DevToolsFrontendAssociatedRequest request);

 private:
  DevToolsFrontendImpl(RenderFrame* render_frame,
                       mojom::DevToolsFrontendAssociatedRequest request);

  // RenderFrameObserver overrides.
  void DidClearWindowObject() override;
  void OnDestruct() override;

  // WebDevToolsFrontendClient implementation.
  void SendMessageToEmbedder(const blink::WebString& message) override;
  bool IsUnderTest() override;

  // mojom::DevToolsFrontend implementation.
  void SetupDevToolsFrontend(
      const std::string& api_script,
      mojom::DevToolsFrontendHostAssociatedPtrInfo host) override;
  void SetupDevToolsExtensionAPI(const std::string& extension_api) override;

  std::unique_ptr<blink::WebDevToolsFrontend> web_devtools_frontend_;
  std::string api_script_;
  mojom::DevToolsFrontendHostAssociatedPtr host_;
  mojo::AssociatedBinding<mojom::DevToolsFrontend> binding_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_FRONTEND_IMPL_H_
