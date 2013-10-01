// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_screenshot_delegate.h"

namespace ash {
namespace test {

TestScreenshotDelegate::TestScreenshotDelegate()
    : handle_take_screenshot_count_(0),
      handle_take_partial_screenshot_count_(0),
      can_take_screenshot_(true) {
}

TestScreenshotDelegate::~TestScreenshotDelegate() {
}

void TestScreenshotDelegate::HandleTakeScreenshotForAllRootWindows() {
  handle_take_screenshot_count_++;
}

void TestScreenshotDelegate::HandleTakePartialScreenshot(
    aura::Window* window, const gfx::Rect& rect) {
  handle_take_partial_screenshot_count_++;
  last_rect_ = rect;
}

bool TestScreenshotDelegate::CanTakeScreenshot() {
  return can_take_screenshot_;
}

}  // namespace test
}  // namespace ash
