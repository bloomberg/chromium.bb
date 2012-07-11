// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_OBSERVER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_OBSERVER_H_

namespace gfx {
class Point;
}

// This observer interface should be implemented in order to receive
// notifications from PanelMouseWatcher when the mouse moves.
class PanelMouseWatcherObserver {
 public:
  // Called when the mouse moves.
  // |mouse_position| is in screen coordinates.
  virtual void OnMouseMove(const gfx::Point& mouse_position) = 0;

 protected:
  virtual ~PanelMouseWatcherObserver() {}
};
#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MOUSE_WATCHER_OBSERVER_H_
