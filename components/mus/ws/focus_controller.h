// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_
#define COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/mus/ws/server_window_drawn_tracker_observer.h"

namespace mus {

namespace ws {

class FocusControllerObserver;
class ServerWindow;
class ServerWindowDrawnTracker;

// Describes the source of the change.
enum class FocusControllerChangeSource {
  EXPLICIT,
  DRAWN_STATE_CHANGED,
};

// Tracks the focused window. Focus is moved to another window when the drawn
// state of the focused window changes.
class FocusController : public ServerWindowDrawnTrackerObserver {
 public:
  FocusController();
  ~FocusController() override;

  // Sets the focused window. Does nothing if |window| is currently focused.
  // This does not notify the delegate.
  void SetFocusedWindow(ServerWindow* window);
  ServerWindow* GetFocusedWindow();

  // Moves activation to the next activatable window.
  void CycleActivationForward();

  void AddObserver(FocusControllerObserver* observer);
  void RemoveObserver(FocusControllerObserver* observer);

 private:
  // Returns whether |window| can be focused or activated.
  bool CanBeFocused(ServerWindow* window) const;
  bool CanBeActivated(ServerWindow* window) const;

  // Implementation of SetFocusedWindow().
  void SetFocusedWindowImpl(FocusControllerChangeSource change_source,
                            ServerWindow* window);

  // ServerWindowDrawnTrackerObserver:
  void OnDrawnStateChanged(ServerWindow* ancestor,
                           ServerWindow* window,
                           bool is_drawn) override;

  base::ObserverList<FocusControllerObserver> observers_;
  scoped_ptr<ServerWindowDrawnTracker> drawn_tracker_;

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_FOCUS_CONTROLLER_H_
