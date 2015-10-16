// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_VIEW_DRAWN_TRACKER_H_
#define COMPONENTS_MUS_WS_SERVER_VIEW_DRAWN_TRACKER_H_

#include <set>

#include "base/basictypes.h"
#include "components/mus/ws/server_view_observer.h"

namespace mus {

class ServerViewDrawnTrackerObserver;

// ServerViewDrawnTracker notifies its observer any time the drawn state of
// the supplied view changes.
//
// NOTE: you must ensure this class is destroyed before the root.
class ServerViewDrawnTracker : public ServerViewObserver {
 public:
  ServerViewDrawnTracker(ServerView* view,
                         ServerViewDrawnTrackerObserver* observer);
  ~ServerViewDrawnTracker() override;

  ServerView* view() { return view_; }

 private:
  void SetDrawn(ServerView* ancestor, bool drawn);

  // Adds |this| as an observer to |view_| and its ancestors.
  void AddObservers();

  // Stops observerving any views we added as an observer in AddObservers().
  void RemoveObservers();

  // ServerViewObserver:
  void OnViewDestroyed(ServerView* view) override;
  void OnViewHierarchyChanged(ServerView* view,
                              ServerView* new_parent,
                              ServerView* old_parent) override;
  void OnViewVisibilityChanged(ServerView* view) override;

  ServerView* view_;
  ServerViewDrawnTrackerObserver* observer_;
  bool drawn_;
  // Set of views we're observing. This is |view_| and all its ancestors.
  std::set<ServerView*> views_;

  DISALLOW_COPY_AND_ASSIGN(ServerViewDrawnTracker);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_VIEW_DRAWN_TRACKER_H_
