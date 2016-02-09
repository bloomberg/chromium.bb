// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_RENDER_PROCESS_OBSERVER_H_
#define COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_RENDER_PROCESS_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/renderer/render_process_observer.h"

namespace web_cache {

// This class filters the incoming cache related control messages.
class WebCacheRenderProcessObserver : public content::RenderProcessObserver {
 public:
  WebCacheRenderProcessObserver();
  ~WebCacheRenderProcessObserver() override;

  // Needs to be called by RenderViews in case of navigations to execute
  // any 'clear cache' commands that were delayed until the next navigation.
  void ExecutePendingClearCache();

 private:
  // RenderProcessObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void WebKitInitialized() override;
  void OnRenderProcessShutdown() override;

  // Message handlers.
  void OnSetCacheCapacities(uint32_t min_dead_capacity,
                            uint32_t max_dead_capacity,
                            uint32_t capacity);
  // If |on_navigation| is true, the clearing is delayed until the next
  // navigation event.
  void OnClearCache(bool on_navigation);

  // If true, the web cache shall be cleared before the next navigation event.
  bool clear_cache_pending_;
  bool webkit_initialized_;
  size_t pending_cache_min_dead_capacity_;
  size_t pending_cache_max_dead_capacity_;
  size_t pending_cache_capacity_;

  DISALLOW_COPY_AND_ASSIGN(WebCacheRenderProcessObserver);
};

}  // namespace web_cache

#endif  // COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_RENDER_PROCESS_OBSERVER_H_

