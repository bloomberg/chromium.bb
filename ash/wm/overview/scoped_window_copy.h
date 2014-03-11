// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_WINDOW_COPY_H_
#define ASH_WM_OVERVIEW_SCOPED_WINDOW_COPY_H_

#include "base/basictypes.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

class CleanupWidgetAfterAnimationObserver;

// ScopedWindowCopy copies a window and will clean up the copied layers after
// the class goes out of scope and the last animation has finished.
class ScopedWindowCopy {
 public:
  ScopedWindowCopy(aura::Window* target_root, aura::Window* src_window);
  ~ScopedWindowCopy();

  aura::Window* GetWindow();

 private:
  // A weak pointer to a copy of the source window owned by cleanup_observer_.
  views::Widget* widget_;

  // A weak pointer to an animation observer which owns itself. When the
  // ScopedWindowCopy is destroyed The animation observer will clean up the
  // widget, layer and itself once any pending animations have completed.
  CleanupWidgetAfterAnimationObserver* cleanup_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowCopy);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_WINDOW_COPY_H_
