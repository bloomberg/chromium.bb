// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/renderer/web_cache_render_process_observer.h"

#include <limits>

#include "components/web_cache/common/web_cache_messages.h"
#include "third_party/WebKit/public/web/WebCache.h"

using blink::WebCache;

namespace web_cache {

namespace {
const size_t kUnitializedCacheCapacity = UINT_MAX;
}

WebCacheRenderProcessObserver::WebCacheRenderProcessObserver()
  : clear_cache_pending_(false),
    webkit_initialized_(false),
    pending_cache_min_dead_capacity_(0),
    pending_cache_max_dead_capacity_(0),
    pending_cache_capacity_(kUnitializedCacheCapacity) {
}

WebCacheRenderProcessObserver::~WebCacheRenderProcessObserver() {
}

void WebCacheRenderProcessObserver::ExecutePendingClearCache() {
  if (clear_cache_pending_ && webkit_initialized_) {
    clear_cache_pending_ = false;
    WebCache::clear();
  }
}

bool WebCacheRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebCacheRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(WebCacheMsg_SetCacheCapacities, OnSetCacheCapacities)
    IPC_MESSAGE_HANDLER(WebCacheMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebCacheRenderProcessObserver::WebKitInitialized() {
  webkit_initialized_ = true;
  if (pending_cache_capacity_ != kUnitializedCacheCapacity) {
    WebCache::setCapacities(pending_cache_min_dead_capacity_,
                            pending_cache_max_dead_capacity_,
                            pending_cache_capacity_);
  }
}

void WebCacheRenderProcessObserver::OnRenderProcessShutdown() {
  webkit_initialized_ = false;
}

void WebCacheRenderProcessObserver::OnSetCacheCapacities(
    size_t min_dead_capacity,
    size_t max_dead_capacity,
    size_t capacity) {
  if (!webkit_initialized_) {
    pending_cache_min_dead_capacity_ = min_dead_capacity;
    pending_cache_max_dead_capacity_ = max_dead_capacity;
    pending_cache_capacity_ = capacity;
    return;
  }

  WebCache::setCapacities(
      min_dead_capacity, max_dead_capacity, capacity);
}

void WebCacheRenderProcessObserver::OnClearCache(bool on_navigation) {
  if (on_navigation || !webkit_initialized_)
    clear_cache_pending_ = true;
  else
    WebCache::clear();
}

}  // namespace web_cache
