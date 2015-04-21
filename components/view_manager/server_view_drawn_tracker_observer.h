// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SERVER_VIEW_DRAWN_TRACKER_OBSERVER_H_
#define COMPONENTS_VIEW_MANAGER_SERVER_VIEW_DRAWN_TRACKER_OBSERVER_H_

namespace view_manager {

class ServerView;

class ServerViewDrawnTrackerObserver {
 public:
  // Invoked when the drawn state changes. If |is_drawn| is false |ancestor|
  // identifies where the change occurred. In the case of a remove |ancestor| is
  // the parent of the view that was removed. In the case of a visibility change
  // |ancestor| is the parent of the view whose visibility changed.
  virtual void OnDrawnStateChanged(ServerView* ancestor,
                                   ServerView* view,
                                   bool is_drawn) {}

 protected:
  virtual ~ServerViewDrawnTrackerObserver() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_SERVER_VIEW_DRAWN_TRACKER_OBSERVER_H_
