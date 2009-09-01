// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/appcache/appcache_frontend_proxy.h"

#include "chrome/common/render_messages.h"

void AppCacheFrontendProxy::OnCacheSelected(int host_id, int64 cache_id ,
                                            appcache::Status status) {
  sender_->Send(new AppCacheMsg_CacheSelected(host_id, cache_id, status));
}

void AppCacheFrontendProxy::OnStatusChanged(const std::vector<int>& host_ids,
                                            appcache::Status status) {
  sender_->Send(new AppCacheMsg_StatusChanged(host_ids, status));
}

void AppCacheFrontendProxy::OnEventRaised(const std::vector<int>& host_ids,
                                          appcache::EventID event_id) {
  sender_->Send(new AppCacheMsg_EventRaised(host_ids, event_id));
}
