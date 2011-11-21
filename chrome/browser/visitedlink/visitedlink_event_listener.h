// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
class VisitedLinkUpdater;

namespace base {
class SharedMemory;
}

class VisitedLinkEventListener : public VisitedLinkMaster::Listener,
                                 public content::NotificationObserver {
 public:
  explicit VisitedLinkEventListener(Profile* profile);
  virtual ~VisitedLinkEventListener();

  virtual void NewTable(base::SharedMemory* table_memory) OVERRIDE;
  virtual void Add(VisitedLinkMaster::Fingerprint fingerprint) OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  void CommitVisitedLinks();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  base::OneShotTimer<VisitedLinkEventListener> coalesce_timer_;
  VisitedLinkCommon::Fingerprints pending_visited_links_;

  content::NotificationRegistrar registrar_;

  // Map between renderer child ids and their VisitedLinkUpdater.
  typedef std::map<int, linked_ptr<VisitedLinkUpdater> > Updaters;
  Updaters updaters_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkEventListener);
};

#endif  // CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
