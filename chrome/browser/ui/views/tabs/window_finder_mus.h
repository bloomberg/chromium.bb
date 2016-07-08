// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_MUS_H_

#include "chrome/browser/ui/views/tabs/window_finder.h"

class WindowFinderMus : public WindowFinder {
 public:
  WindowFinderMus();
  ~WindowFinderMus() override;

  // Overridden from WindowFinder:
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowFinderMus);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_MUS_H_
