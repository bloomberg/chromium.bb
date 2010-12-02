// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VisitedLinkEventListener broadcasts link coloring database updates to all
// processes. It also coalesces the updates to avoid excessive broadcasting of
// messages to the renderers.

#ifndef CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
#define CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"

namespace base {
class SharedMemory;
}

class VisitedLinkEventListener : public VisitedLinkMaster::Listener {
 public:
  VisitedLinkEventListener();
  virtual ~VisitedLinkEventListener();

  virtual void NewTable(base::SharedMemory* table_memory);
  virtual void Add(VisitedLinkMaster::Fingerprint fingerprint);
  virtual void Reset();

 private:
  void CommitVisitedLinks();

  base::OneShotTimer<VisitedLinkEventListener> coalesce_timer_;
  VisitedLinkCommon::Fingerprints pending_visited_links_;

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkEventListener);
};

#endif  // CHROME_BROWSER_VISITEDLINK_VISITEDLINK_EVENT_LISTENER_H_
