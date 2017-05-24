// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_TASKBAR_ICON_FINDER_H_
#define CHROME_BROWSER_WIN_TASKBAR_ICON_FINDER_H_

#include "ui/gfx/geometry/rect.h"

// Finds the bounding rectangle of Chrome's taskbar icon on the primary monitor.
// Returns an empty rect in case of error or if no icon can be found. This is a
// blocking call.
gfx::Rect FindTaskbarIconModal();

#endif  // CHROME_BROWSER_WIN_TASKBAR_ICON_FINDER_H_
