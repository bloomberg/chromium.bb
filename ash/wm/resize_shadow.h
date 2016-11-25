// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_H_
#define ASH_WM_RESIZE_SHADOW_H_

#include <memory>

#include "base/macros.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}
namespace wm {
class ImageGrid;
}

namespace ash {

// A class to render the resize edge effect when the user moves their mouse
// over a sizing edge.  This is just a visual effect; the actual resize is
// handled by the EventFilter.
class ResizeShadow {
 public:
  ResizeShadow();
  ~ResizeShadow();

  // Initializes the resize effect layers for a given |window|.
  void Init(aura::Window* window);

  // Shows resize effects for one or more edges based on a |hit_test| code, such
  // as HTRIGHT or HTBOTTOMRIGHT.
  void ShowForHitTest(int hit_test);

  // Hides all resize effects.
  void Hide();

  // Updates the effect positions based on the |bounds| of the window.
  void Layout(const gfx::Rect& bounds);

  int GetLastHitTestForTest() const { return last_hit_test_; }

 private:
  // Images for the shadow effect.
  std::unique_ptr<::wm::ImageGrid> image_grid_;

  // Hit test value from last call to ShowForHitTest().  Used to prevent
  // repeatedly triggering the same animations for the same hit.
  int last_hit_test_;

  DISALLOW_COPY_AND_ASSIGN(ResizeShadow);
};

}  // namespace ash

#endif  // ASH_WM_RESIZE_SHADOW_H_
