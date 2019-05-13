// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webapplicationcachehost_impl.h"

#include <string>

#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebApplicationCacheHostClient;
using blink::WebConsoleMessage;

namespace content {

RendererWebApplicationCacheHostImpl::RendererWebApplicationCacheHostImpl(
    RenderFrameImpl* render_frame,
    WebApplicationCacheHostClient* client,
    int appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : WebApplicationCacheHostImpl(render_frame->GetDocumentInterfaceBroker(),
                                  client,
                                  appcache_host_id,
                                  std::move(task_runner)),
      routing_id_(render_frame->render_view()->GetRoutingID()),
      frame_routing_id_(render_frame->GetRoutingID()) {}

void RendererWebApplicationCacheHostImpl::LogMessage(
    blink::mojom::ConsoleMessageLevel log_level,
    const std::string& message) {
  if (RenderThreadImpl::current()->web_test_mode())
    return;

  RenderViewImpl* render_view = GetRenderView();
  if (!render_view || !render_view->webview() ||
      !render_view->webview()->MainFrame())
    return;

  blink::WebFrame* frame = render_view->webview()->MainFrame();
  if (!frame->IsWebLocalFrame())
    return;
  // TODO(michaeln): Make app cache host per-frame and correctly report to the
  // involved frame.
  frame->ToWebLocalFrame()->AddMessageToConsole(WebConsoleMessage(
      static_cast<blink::mojom::ConsoleMessageLevel>(log_level),
      blink::WebString::FromUTF8(message.c_str())));
}

void RendererWebApplicationCacheHostImpl::SetSubresourceFactory(
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(frame_routing_id_);
  if (render_frame) {
    auto info = std::make_unique<ChildURLLoaderFactoryBundleInfo>();
    info->appcache_factory_info() = url_loader_factory.PassInterface();
    render_frame->GetLoaderFactoryBundle()->Update(std::move(info));
  }
}

RenderViewImpl* RendererWebApplicationCacheHostImpl::GetRenderView() {
  return RenderViewImpl::FromRoutingID(routing_id_);
}

}  // namespace content
