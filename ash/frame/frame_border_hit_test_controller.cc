// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/frame_border_hit_test_controller.h"

#include <memory>

#include "ash/wm/resize_handle_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

FrameBorderHitTestController::FrameBorderHitTestController(views::Widget* frame)
    : frame_window_(frame->GetNativeWindow()) {
  frame_window_->SetEventTargeter(std::unique_ptr<ui::EventTargeter>(
      new ResizeHandleWindowTargeter(frame_window_, NULL)));
}

FrameBorderHitTestController::~FrameBorderHitTestController() {}

}  // namespace ash
