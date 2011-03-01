// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_browsing_context.h"

#include "base/message_loop.h"
#include "base/singleton.h"
#include "chrome/common/notification_service.h"
#include "content/browser/browser_thread.h"

CPBrowsingContextManager* CPBrowsingContextManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<CPBrowsingContextManager>::get();
}

CPBrowsingContextManager::CPBrowsingContextManager() {
  registrar_.Add(this, NotificationType::URL_REQUEST_CONTEXT_RELEASED,
                 NotificationService::AllSources());
}

CPBrowsingContextManager::~CPBrowsingContextManager() {
}

CPBrowsingContext CPBrowsingContextManager::Allocate(
    net::URLRequestContext* context) {
  int32 map_id = map_.Add(context);
  return static_cast<CPBrowsingContext>(map_id);
}

net::URLRequestContext* CPBrowsingContextManager::ToURLRequestContext(
    CPBrowsingContext id) {
  return map_.Lookup(static_cast<int32>(id));
}

CPBrowsingContext CPBrowsingContextManager::Lookup(
    net::URLRequestContext* context) {
  ReverseMap::const_iterator it = reverse_map_.find(context);
  if (it == reverse_map_.end()) {
    CPBrowsingContext id = Allocate(context);
    reverse_map_[context] = id;
    return id;
  } else {
    return it->second;
  }
}

void CPBrowsingContextManager::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(type == NotificationType::URL_REQUEST_CONTEXT_RELEASED);

  net::URLRequestContext* context =
      Source<net::URLRequestContext>(source).ptr();

  // Multiple CPBrowsingContexts may refer to the same net::URLRequestContext.
  for (Map::iterator it(&map_); !it.IsAtEnd(); it.Advance()) {
    if (it.GetCurrentValue() == context)
      map_.Remove(it.GetCurrentKey());
  }

  reverse_map_.erase(context);
}
