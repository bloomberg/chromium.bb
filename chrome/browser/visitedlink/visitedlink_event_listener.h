// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VisitedLinkEventListener broadcasts link coloring database updates to all
// processes. It also coalesces the updates to avoid excessive broadcasting of
// messages to the renderers.

#ifndef CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
#define CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
#pragma once

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/timer.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class VisitedLinkUpdater;

namespace base {
class SharedMemory;
}

class VisitedLinkEventListener : public VisitedLinkMaster::Listener,
                                 public NotificationObserver {
 public:
  VisitedLinkEventListener();
  virtual ~VisitedLinkEventListener();

  virtual void NewTable(base::SharedMemory* table_memory);
  virtual void Add(VisitedLinkMaster::Fingerprint fingerprint);
  virtual void Reset();

 private:
  void CommitVisitedLinks();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  base::OneShotTimer<VisitedLinkEventListener> coalesce_timer_;
  VisitedLinkCommon::Fingerprints pending_visited_links_;

  NotificationRegistrar registrar_;

  // Map between renderer child ids and their VisitedLinkUpdater.
  typedef std::map<int, linked_ptr<VisitedLinkUpdater> > Updaters;
  Updaters updaters_;

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkEventListener);
};

#endif  // CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
