// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/drag_window_resizer.h"

namespace mash {
namespace wm {

DragWindowResizer::DragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    ash::wm::WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(std::move(next_window_resizer)) {}

DragWindowResizer::~DragWindowResizer() {
  if (window_state_)
    window_state_->DeleteDragDetails();
}

void DragWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  next_window_resizer_->Drag(location, event_flags);
  // http://crbug.com/613199.
  NOTIMPLEMENTED();
}

void DragWindowResizer::CompleteDrag() {
  next_window_resizer_->CompleteDrag();
  // http://crbug.com/613199.
  NOTIMPLEMENTED();
}

void DragWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  // http://crbug.com/613199.
  NOTIMPLEMENTED();
}

}  // namespace wm
}  // namespace mash
