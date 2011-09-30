// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_

namespace gfx {
class Point;
}

// This is the base class for functionality to watch for mouse movements.
// The specific implementation of this abstract class differ in how they
// track mouse movements.
class PanelMouseWatcher {
 public:
  // Observer interface for watching mouse movement.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the mouse moves.
    virtual void OnMouseMove(const gfx::Point& mouse_position) = 0;
  };

  // Returns an instance of the platform specific implementation.
  static PanelMouseWatcher* Create(Observer* observer);

  // clang gives an error asking for an out of line destructor.
  virtual ~PanelMouseWatcher();

  // Start/stop tracking mouse movements.
  virtual void Start() = 0;
  virtual void Stop() = 0;

 protected:
  explicit PanelMouseWatcher(Observer* observer);

  void NotifyMouseMovement(const gfx::Point& mouse_position);

 private:
  Observer* observer_;
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_
