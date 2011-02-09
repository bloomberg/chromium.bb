// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webapplicationcachehost_impl.h"

#include "chrome/common/content_settings_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using appcache::AppCacheBackend;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebConsoleMessage;

RendererWebApplicationCacheHostImpl::RendererWebApplicationCacheHostImpl(
    RenderView* render_view,
    WebApplicationCacheHostClient* client,
    AppCacheBackend* backend)
    : WebApplicationCacheHostImpl(client, backend),
      content_blocked_(false),
      routing_id_(render_view->routing_id()) {
}

void RendererWebApplicationCacheHostImpl::OnLogMessage(
    appcache::LogLevel log_level, const std::string& message) {
  RenderView* render_view = GetRenderView();
  if (!render_view || !render_view->webview() ||
      !render_view->webview()->mainFrame())
    return;

  WebKit::WebFrame* frame = render_view->webview()->mainFrame();
  frame->addMessageToConsole(WebConsoleMessage(
        static_cast<WebConsoleMessage::Level>(log_level),
        WebKit::WebString::fromUTF8(message.c_str())));
}

void RendererWebApplicationCacheHostImpl::OnContentBlocked(
    const GURL& manifest_url) {
  RenderThread::current()->Send(new ViewHostMsg_AppCacheAccessed(
      routing_id_, manifest_url, true));
}

void RendererWebApplicationCacheHostImpl::OnCacheSelected(
    const appcache::AppCacheInfo& info) {
  if (!info.manifest_url.is_empty()) {
    RenderThread::current()->Send(new ViewHostMsg_AppCacheAccessed(
        routing_id_, info.manifest_url, false));
  }
  WebApplicationCacheHostImpl::OnCacheSelected(info);
}

RenderView* RendererWebApplicationCacheHostImpl::GetRenderView() {
  return static_cast<RenderView*>
      (RenderThread::current()->ResolveRoute(routing_id_));
}
