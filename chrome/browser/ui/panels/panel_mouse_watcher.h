// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_

#include "base/hash_tables.h"

class NativePanel;

namespace gfx {

class Point;

}

#if defined(COMPILER_GCC)

// Defines the hash function for type NativePanel* to be the pointer value.
// Need this to make base::hash_set<NativePanel*> work.  Borrowed the idea from
// prerender_manager.h.
namespace __gnu_cxx {
template <>
struct hash<NativePanel*> {
  std::size_t operator()(NativePanel* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
}

#endif

// This is the base class for functionality to respond to mouse movements when
// at least one panel is in the minimized state.  On detecting the mouse
// movement, it interacts with PanelManager to either show the titlebar or go
// to minimized state.  The specific implementation of this abstract class
// differ in how they track mouse movements.
class PanelMouseWatcher {
 public:
  // Returns an instance of the platform specific implementation.
  static PanelMouseWatcher* GetInstance();

  // clang gives an error asking for an out of line destructor.
  virtual ~PanelMouseWatcher();

  // Adds to set of subscribers. Starts the mouse watcher when the first
  // subscriber is registered.
  void AddSubscriber(NativePanel* native_panel);

  // Removes from set of subscribers. Stops the mouse watcher when the last
  // subscriber is unregistered.
  void RemoveSubscriber(NativePanel* native_panel);

  bool IsSubscribed(NativePanel* native_panel) const;

  // To be used by tests only. Allows unit testing to be done without the
  // timer running.
  void EnableTestingMode();

  // Helper function invoked when mouse movement is detected. Factoring out this
  // function allows for unit testing without actually tracking the mouse
  // movements.
  void HandleMouseMovement(const gfx::Point& mouse_position);

 protected:
  PanelMouseWatcher();

  // Starts tracking mouse movements.
  virtual void Start() = 0;

  // Stops tracking mouse movements.
  virtual void Stop() = 0;

 private:
  // Stores the action requested of PanelManager on the last timer callback.
  // Used to avoid sending redundant requests to PanelManager.
  bool bring_up_titlebar_;

  // The set of subscribers currently registered. The mouse watcher is started
  // when the first subscriber is registered and stopped when the last
  // subscriber is unregistered.
  base::hash_set<NativePanel*> subscribers_;

  // If set to true, ignore all Start/Stop requests. Used for unit testing.
  bool is_testing_mode_enabled_;
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_
