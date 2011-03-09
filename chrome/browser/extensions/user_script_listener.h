// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#pragma once

#include <list>

#include "base/ref_counted.h"
#include "content/browser/renderer_host/resource_queue.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace net {
class URLRequest;
}  // namespace net

class Extension;
class URLPattern;
struct GlobalRequestID;

// This class handles delaying of resource loads that depend on unloaded user
// scripts. For each request that comes in, we check if it depends on a user
// script, and if so, whether that user script is ready; if not, we delay the
// request.
//
// This class lives mostly on the IO thread. It listens on the UI thread for
// updates to loaded extensions.
class UserScriptListener
    : public base::RefCountedThreadSafe<UserScriptListener>,
      public ResourceQueueDelegate,
      public NotificationObserver {
 public:
  explicit UserScriptListener(ResourceQueue* resource_queue);

  // Call this to do necessary cleanup on the main thread before the object
  // is deleted.
  void ShutdownMainThread();

  // ResourceQueueDelegate:
  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id);
  virtual void WillShutdownResourceQueue();

 private:
  friend class base::RefCountedThreadSafe<UserScriptListener>;

  typedef std::list<URLPattern> URLPatterns;

  ~UserScriptListener();

  // Resume any requests that we delayed in order to wait for user scripts.
  void StartDelayedRequests();

  // Appends new url patterns to our list, also setting user_scripts_ready_
  // to false.
  void AppendNewURLPatterns(const URLPatterns& new_patterns);

  // Replaces our url pattern list. This is only used when patterns have been
  // deleted, so user_scripts_ready_ remains unchanged.
  void ReplaceURLPatterns(const URLPatterns& patterns);

  ResourceQueue* resource_queue_;

  // A list of every request that we delayed. Will be flushed when user scripts
  // are ready.
  typedef std::list<GlobalRequestID> DelayedRequests;
  DelayedRequests delayed_request_ids_;

  // TODO(mpcomplete): the rest of this stuff should really be per-profile, but
  // the complexity doesn't seem worth it at this point.

  // True if the user scripts contained in |url_patterns_| are ready for
  // injection.
  bool user_scripts_ready_;

  // A list of URL patterns that have will have user scripts applied to them.
  URLPatterns url_patterns_;

  // --- UI thread:

  // Helper to collect the extension's user script URL patterns in a list and
  // return it.
  void CollectURLPatterns(const Extension* extension, URLPatterns* patterns);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptListener);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
