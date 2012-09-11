// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"

namespace gfx {
class Point;
}

class PanelMouseWatcherObserver;

// This is the base class for functionality to watch for mouse movements.
// The specific implementation of this abstract class differ in how they
// track mouse movements.
class PanelMouseWatcher {
 public:
  // Returns an instance of the platform specific implementation.
  static PanelMouseWatcher* Create();

  // clang gives an error asking for an out of line destructor.
  virtual ~PanelMouseWatcher();

  void AddObserver(PanelMouseWatcherObserver* observer);
  void RemoveObserver(PanelMouseWatcherObserver* observer);

  // Returns current mouse position. This may be different from the
  // mouse position in NotifyMouseMovement.
  virtual gfx::Point GetMousePosition() const = 0;

 protected:
  PanelMouseWatcher();

  // |mouse_position| is in screen coordinates.
  virtual void NotifyMouseMovement(const gfx::Point& mouse_position);

  // Returns true if watching mouse movements.
  virtual bool IsActive() const = 0;

 private:
  friend class PanelMouseWatcherTest;
  FRIEND_TEST_ALL_PREFIXES(PanelMouseWatcherTest, StartStopWatching);
  friend class BasePanelBrowserTest;

  // Start/stop tracking mouse movements.
  virtual void Start() = 0;
  virtual void Stop() = 0;

  ObserverList<PanelMouseWatcherObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_H_
