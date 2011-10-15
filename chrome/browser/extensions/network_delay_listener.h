// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_NETWORK_DELAY_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_NETWORK_DELAY_LISTENER_H_
#pragma once

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/perftimer.h"
#include "base/time.h"
#include "content/browser/renderer_host/resource_queue.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace net {
class URLRequest;
}  // namespace net

struct GlobalRequestID;

// This class handles delaying of resource loads that depend on unloaded
// extensions. For each request that comes in, we check if all extensions are
// ready for it to be loaded; if not, we delay the request.
//
// This class lives mostly on the IO thread. It listens on the UI thread for
// updates to loaded extensions.  It will delete itself on the UI thread after
// WillShutdownResourceQueue is called (on the IO thread).
class NetworkDelayListener
    : public base::RefCountedThreadSafe<NetworkDelayListener>,
      public ResourceQueueDelegate,
      public NotificationObserver {
 public:
  NetworkDelayListener();

 private:
  // ResourceQueueDelegate:
  virtual void Initialize(ResourceQueue* resource_queue) OVERRIDE;
  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id) OVERRIDE;
  virtual void WillShutdownResourceQueue() OVERRIDE;

  friend class base::RefCountedThreadSafe<NetworkDelayListener>;

  virtual ~NetworkDelayListener();

  // Called when an extension that wants to delay network requests is loading.
  void OnExtensionPending(const std::string& id);

  // Called when an extension that wants to delay network requests is ready.
  void OnExtensionReady(const std::string& id);

  // If there are no more extensions pending, tell the network queue to resume
  // processing requests.
  void StartDelayedRequestsIfReady();

  // Cleanup on UI thread.
  void Cleanup();

  ResourceQueue* resource_queue_;

  // TODO(mpcomplete, pamg): the rest of this stuff should really be
  // per-profile, but the complexity doesn't seem worth it at this point.

  // True if the extensions are ready for network requests to proceed. In
  // practice this means that the background pages of any pending extensions
  // have been run.
  bool extensions_ready_;

  // Which extension IDs have registered to delay network requests on startup,
  // but are not yet ready for them to resume.
  std::set<std::string> pending_extensions_;

  // Used to measure how long each extension has taken to become ready.
  std::map<std::string, base::TimeTicks> delay_start_times_;

  // Used to measure total time between the first delay request and resuming
  // network requests.
  PerfTimer overall_start_time_;

  // Whether we've already calculated total startup time, so we don't corrupt
  // the data with entries from installing or enabling single extensions.
  bool recorded_startup_delay_;

  // --- UI thread:

  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDelayListener);
};

#endif  // CHROME_BROWSER_EXTENSIONS_NETWORK_DELAY_LISTENER_H_
