// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_OBSERVER_H_
#define ASH_MUS_WINDOW_OBSERVER_H_

#include <stdint.h>

namespace ui {
class Event;
}

namespace ash {
namespace mus {

class RootWindowController;

class WindowManagerObserver {
 public:
  // Called when the WindowTreeClient associated with the WindowManager is
  // about to be destroyed.
  virtual void OnWindowTreeClientDestroyed() {}

  virtual void OnAccelerator(uint32_t id, const ui::Event& event) {}

  virtual void OnRootWindowControllerAdded(RootWindowController* controller) {}
  virtual void OnWillDestroyRootWindowController(
      RootWindowController* controller) {}

 protected:
  virtual ~WindowManagerObserver() {}
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_OBSERVER_H_
