// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_
#define COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/ws/server_window_drawn_tracker_observer.h"

namespace mus {

class FocusControllerDelegate;
class ServerWindow;
class ServerWindowDrawnTracker;

// Tracks a focused window. Focus is moved to another window when the drawn
// state
// of the focused window changes and the delegate is notified.
class FocusController : public ServerWindowDrawnTrackerObserver {
 public:
  explicit FocusController(FocusControllerDelegate* delegate);
  ~FocusController() override;

  // Sets the focused window. Does nothing if |window| is currently focused.
  // This
  // does not notify the delegate.
  void SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();

 private:
  // Describes the source of the change.
  enum ChangeSource {
    CHANGE_SOURCE_EXPLICIT,
    CHANGE_SOURCE_DRAWN_STATE_CHANGED,
  };

  // Implementation of SetFocusedWindow().
  void SetFocusedWindowImpl(ServerWindow* window, ChangeSource change_source);

  // ServerWindowDrawnTrackerObserver:
  void OnDrawnStateChanged(ServerWindow* ancestor,
                           ServerWindow* window,
                           bool is_drawn) override;

  FocusControllerDelegate* delegate_;
  scoped_ptr<ServerWindowDrawnTracker> drawn_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_
