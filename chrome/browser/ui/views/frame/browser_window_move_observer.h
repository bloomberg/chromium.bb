// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_MOVE_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_MOVE_OBSERVER_H_
#pragma once

// BrowserView notifies BrowserWindowMoveObserver as the window moves.
class BrowserWindowMoveObserver {
 public:
  virtual void OnWidgetMoved() = 0;

 protected:
  ~BrowserWindowMoveObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_MOVE_OBSERVER_H_
