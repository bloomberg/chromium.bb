// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SERVER_VIEW_DRAWN_TRACKER_OBSERVER_H_
#define COMPONENTS_MUS_WS_SERVER_VIEW_DRAWN_TRACKER_OBSERVER_H_

namespace mus {

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

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SERVER_VIEW_DRAWN_TRACKER_OBSERVER_H_
