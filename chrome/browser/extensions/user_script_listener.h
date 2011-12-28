// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#pragma once

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/resource_queue.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
struct GlobalRequestID;
}

namespace net {
class URLRequest;
}  // namespace net

class Extension;
class URLPattern;

// This class handles delaying of resource loads that depend on unloaded user
// scripts. For each request that comes in, we check if it depends on a user
// script, and if so, whether that user script is ready; if not, we delay the
// request.
//
// This class lives mostly on the IO thread. It listens on the UI thread for
// updates to loaded extensions. It will delete itself on the UI thread after
// WillShutdownResourceQueue is called (on the IO thread).
class UserScriptListener
    : public base::RefCountedThreadSafe<UserScriptListener>,
      public ResourceQueueDelegate,
      public content::NotificationObserver {
 public:
  UserScriptListener();

 private:
  // ResourceQueueDelegate:
  virtual void Initialize(ResourceQueue* resource_queue) OVERRIDE;
  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const content::GlobalRequestID& request_id) OVERRIDE;
  virtual void WillShutdownResourceQueue() OVERRIDE;

  friend class base::RefCountedThreadSafe<UserScriptListener>;

  typedef std::list<URLPattern> URLPatterns;

  virtual ~UserScriptListener();

  // Update user_scripts_ready_ based on the status of all profiles. On a
  // transition from false to true, we resume all delayed requests.
  void CheckIfAllUserScriptsReady();

  // Resume any requests that we delayed in order to wait for user scripts.
  void UserScriptsReady(void* profile_id);

  // Clean up per-profile information related to the given profile.
  void ProfileDestroyed(void* profile_id);

  // Appends new url patterns to our list, also setting user_scripts_ready_
  // to false.
  void AppendNewURLPatterns(void* profile_id, const URLPatterns& new_patterns);

  // Replaces our url pattern list. This is only used when patterns have been
  // deleted, so user_scripts_ready_ remains unchanged.
  void ReplaceURLPatterns(void* profile_id, const URLPatterns& patterns);

  // Cleanup on UI thread.
  void Cleanup();

  ResourceQueue* resource_queue_;

  // True if all user scripts from all profiles are ready.
  bool user_scripts_ready_;

  // Per-profile bookkeeping so we know when all user scripts are ready.
  struct ProfileData;
  typedef std::map<void*, ProfileData> ProfileDataMap;
  ProfileDataMap profile_data_;

  // --- UI thread:

  // Helper to collect the extension's user script URL patterns in a list and
  // return it.
  void CollectURLPatterns(const Extension* extension, URLPatterns* patterns);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptListener);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
