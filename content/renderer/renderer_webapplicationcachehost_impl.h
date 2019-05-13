// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_

#include "content/renderer/appcache/web_application_cache_host_impl.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {
class RenderFrameImpl;
class RenderViewImpl;

class RendererWebApplicationCacheHostImpl : public WebApplicationCacheHostImpl {
 public:
  RendererWebApplicationCacheHostImpl(
      RenderFrameImpl* render_frame,
      blink::WebApplicationCacheHostClient* client,
      int appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // blink::mojom::AppCacheHostFrontend:
  void LogMessage(blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message) override;

  void SetSubresourceFactory(
      network::mojom::URLLoaderFactoryPtr url_loader_factory) override;

 private:
  RenderViewImpl* GetRenderView();

  int routing_id_;
  int frame_routing_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
