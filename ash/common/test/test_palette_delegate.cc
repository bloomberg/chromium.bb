// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_palette_delegate.h"

namespace ash {

TestPaletteDelegate::TestPaletteDelegate() {}

TestPaletteDelegate::~TestPaletteDelegate() {}

void TestPaletteDelegate::CreateNote() {
  ++create_note_count_;
}

bool TestPaletteDelegate::HasNoteApp() {
  ++has_note_app_count_;
  return has_note_app_;
}

void TestPaletteDelegate::SetPartialMagnifierState(bool enabled) {
  partial_magnifier_state_ = enabled;
}

void TestPaletteDelegate::TakeScreenshot() {
  ++take_screenshot_count_;
}

void TestPaletteDelegate::TakePartialScreenshot() {
  ++take_partial_screenshot_count_;
}

}  // namespace ash
