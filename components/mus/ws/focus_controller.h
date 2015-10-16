// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_
#define COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/ws/server_view_drawn_tracker_observer.h"

namespace mus {

class FocusControllerDelegate;
class ServerView;
class ServerViewDrawnTracker;

// Tracks a focused view. Focus is moved to another view when the drawn state
// of the focused view changes and the delegate is notified.
class FocusController : public ServerViewDrawnTrackerObserver {
 public:
  explicit FocusController(FocusControllerDelegate* delegate);
  ~FocusController() override;

  // Sets the focused view. Does nothing if |view| is currently focused. This
  // does not notify the delegate.
  void SetFocusedView(ServerView* view);
  ServerView* GetFocusedView();

 private:
  // Describes the source of the change.
  enum ChangeSource {
    CHANGE_SOURCE_EXPLICIT,
    CHANGE_SOURCE_DRAWN_STATE_CHANGED,
  };

  // Implementation of SetFocusedView().
  void SetFocusedViewImpl(ServerView* view, ChangeSource change_source);

  // ServerViewDrawnTrackerObserver:
  void OnDrawnStateChanged(ServerView* ancestor,
                           ServerView* view,
                           bool is_drawn) override;

  FocusControllerDelegate* delegate_;
  scoped_ptr<ServerViewDrawnTracker> drawn_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_
