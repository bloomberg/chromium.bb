// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/appcache/appcache_dispatcher.h"

#include "chrome/common/render_messages.h"
#include "webkit/appcache/web_application_cache_host_impl.h"

bool AppCacheDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppCacheDispatcher, msg)
    IPC_MESSAGE_HANDLER(AppCacheMsg_CacheSelected, OnCacheSelected)
    IPC_MESSAGE_HANDLER(AppCacheMsg_StatusChanged, OnStatusChanged)
    IPC_MESSAGE_HANDLER(AppCacheMsg_EventRaised, OnEventRaised)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppCacheDispatcher::OnCacheSelected(int host_id, int64 cache_id,
                                         appcache::Status status) {
  frontend_impl_.OnCacheSelected(host_id, cache_id, status);
}

void AppCacheDispatcher::OnStatusChanged(const std::vector<int>& host_ids,
                                         appcache::Status status) {
  frontend_impl_.OnStatusChanged(host_ids, status);
}

void AppCacheDispatcher::OnEventRaised(const std::vector<int>& host_ids,
                                       appcache::EventID event_id) {
  frontend_impl_.OnEventRaised(host_ids, event_id);
}
