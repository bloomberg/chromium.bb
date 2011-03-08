// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_PLUGIN_BROWSING_CONTEXT_H_
#define CHROME_BROWSER_CHROME_PLUGIN_BROWSING_CONTEXT_H_
#pragma once

#include <map>

#include "base/id_map.h"
#include "chrome/common/chrome_plugin_api.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace net {
class URLRequestContext;
}  // namespace net

// This class manages the mapping between CPBrowsingContexts and
// net::URLRequestContexts.  It observes when net::URLRequestContexts go away,
// and invalidates the corresponding CPBrowsingContexts.  CPBrowsingContexts can
// be associated with other data as well, so there can be multiple ones
// referring to a given net::URLRequestContext.
// Note: This class should be used on the IO thread only.
class CPBrowsingContextManager : public NotificationObserver {
 public:
  static CPBrowsingContextManager* GetInstance();

  // Note: don't call these directly - use Instance() above.  They are public
  // so Singleton can access them.
  CPBrowsingContextManager();
  ~CPBrowsingContextManager();

  // Generate a new unique CPBrowsingContext ID from the given
  // net::URLRequestContext.  Multiple CPBrowsingContexts can map to the same
  // net::URLRequestContext.
  CPBrowsingContext Allocate(net::URLRequestContext* context);

  // Return the net::URLRequestContext that this CPBrowsingContext refers to, or
  // NULL if not found.
  net::URLRequestContext* ToURLRequestContext(CPBrowsingContext id);

  // Return a CPBrowsingContext ID that corresponds to the given
  // net::URLRequestContext. This function differs from Allocate in that calling
  // this multiple times with the same argument gives the same ID.
  CPBrowsingContext Lookup(net::URLRequestContext* context);

 private:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  typedef IDMap<net::URLRequestContext> Map;
  typedef std::map<net::URLRequestContext*, CPBrowsingContext> ReverseMap;

  NotificationRegistrar registrar_;

  Map map_;  // map of CPBrowsingContext -> net::URLRequestContext
  ReverseMap reverse_map_; // map of net::URLRequestContext -> CPBrowsingContext
};

#endif  // CHROME_BROWSER_CHROME_PLUGIN_BROWSING_CONTEXT_H_
