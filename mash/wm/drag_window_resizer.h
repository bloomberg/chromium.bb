// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_DRAG_WINDOW_RESIZER_H_
#define MASH_WM_DRAG_WINDOW_RESIZER_H_

#include <memory>

#include "ash/wm/common/window_resizer.h"
#include "base/macros.h"

namespace mash {
namespace wm {

// DragWindowResizer is a decorator of WindowResizer and adds the ability to
// drag windows across displays.
class DragWindowResizer : public ash::WindowResizer {
 public:
  DragWindowResizer(std::unique_ptr<WindowResizer> next_window_resizer,
                    ash::wm::WindowState* window_state);
  ~DragWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::Point& location, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;

 private:
  std::unique_ptr<ash::WindowResizer> next_window_resizer_;

  DISALLOW_COPY_AND_ASSIGN(DragWindowResizer);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_DRAG_WINDOW_RESIZER_H_
