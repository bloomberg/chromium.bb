// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_LISTENER_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_LISTENER_H_

#include <map>
#include <vector>

#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/resource_queue.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class BlacklistManager;

// Delays requests until privacy blacklists are ready.
class BlacklistListener
    : public ResourceQueueDelegate,
      public NotificationObserver,
      public base::RefCountedThreadSafe<BlacklistListener,
                                        ChromeThread::DeleteOnUIThread> {
 public:
  // UI THREAD ONLY ------------------------------------------------------------

  explicit BlacklistListener(ResourceQueue* resource_queue);
  virtual ~BlacklistListener();

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // IO THREAD ONLY ------------------------------------------------------------

  // ResourceQueueDelegate:
  virtual bool ShouldDelayRequest(
      URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id);
  virtual void WillShutdownResourceQueue();

 private:
  friend class ChromeThread;
  friend class DeleteTask<BlacklistListener>;

  // Tell our resource queue to start the requests we requested to be delayed.
  void StartDelayedRequests(BlacklistManager* blacklist_manager);

  // Resource queue we're delegate of.
  ResourceQueue* resource_queue_;

  typedef std::vector<GlobalRequestID> RequestList;
  typedef std::map<const BlacklistManager*, RequestList> DelayedRequestMap;

  // Keep track of requests we have requested to delay so that we can resume
  // them as the blacklists load (note that we have a request list per blacklist
  // manager).
  DelayedRequestMap delayed_requests_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistListener);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_LISTENER_H_
