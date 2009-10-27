// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_browsing_context.h"

#include "base/message_loop.h"
#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_service.h"

CPBrowsingContextManager* CPBrowsingContextManager::Instance() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return Singleton<CPBrowsingContextManager>::get();
}

CPBrowsingContextManager::CPBrowsingContextManager() {
  registrar_.Add(this, NotificationType::URL_REQUEST_CONTEXT_RELEASED,
                 NotificationService::AllSources());
}

CPBrowsingContextManager::~CPBrowsingContextManager() {
}

CPBrowsingContext CPBrowsingContextManager::Allocate(
    URLRequestContext* context) {
  int32 map_id = map_.Add(context);
  return static_cast<CPBrowsingContext>(map_id);
}

URLRequestContext* CPBrowsingContextManager::ToURLRequestContext(
    CPBrowsingContext id) {
  return map_.Lookup(static_cast<int32>(id));
}

CPBrowsingContext CPBrowsingContextManager::Lookup(URLRequestContext* context) {
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

  URLRequestContext* context = Source<URLRequestContext>(source).ptr();

  // Multiple CPBrowsingContexts may refer to the same URLRequestContext.
  for (Map::iterator it(&map_); !it.IsAtEnd(); it.Advance()) {
    if (it.GetCurrentValue() == context)
      map_.Remove(it.GetCurrentKey());
  }

  reverse_map_.erase(context);
}
