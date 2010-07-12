// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_

#include "webkit/appcache/web_application_cache_host_impl.h"

class RenderView;

class RendererWebApplicationCacheHostImpl
    : public appcache::WebApplicationCacheHostImpl {
 public:
  RendererWebApplicationCacheHostImpl(
      RenderView* render_view,
      WebKit::WebApplicationCacheHostClient* client,
      appcache::AppCacheBackend* backend);

  // appcache::WebApplicationCacheHostImpl methods.
  virtual void OnLogMessage(appcache::LogLevel log_level,
                            const std::string& message);
  virtual void OnContentBlocked(const GURL& manifest_url);
  virtual void OnCacheSelected(int64 selected_cache_id,
                               appcache::Status status);

 private:
  RenderView* GetRenderView();

  bool content_blocked_;
  int routing_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
