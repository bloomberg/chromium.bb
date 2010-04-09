// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webapplicationcachehost_impl.h"

#include "chrome/common/content_settings_types.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"

using appcache::AppCacheBackend;
using WebKit::WebApplicationCacheHostClient;

RendererWebApplicationCacheHostImpl::RendererWebApplicationCacheHostImpl(
    RenderView* render_view,
    WebApplicationCacheHostClient* client,
    AppCacheBackend* backend)
    : WebApplicationCacheHostImpl(client, backend),
      content_blocked_(false),
      routing_id_(render_view->routing_id()) {
}

RendererWebApplicationCacheHostImpl::~RendererWebApplicationCacheHostImpl() {
}

void RendererWebApplicationCacheHostImpl::OnContentBlocked() {
  if (!content_blocked_) {
    RenderThread::current()->Send(new ViewHostMsg_ContentBlocked(
        routing_id_, CONTENT_SETTINGS_TYPE_COOKIES));
    content_blocked_ = true;
  }
}
