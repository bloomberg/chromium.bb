// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/test_screen_manager_delegate.h"

#include "ui/aura/test/test_screen.h"

namespace athena {
namespace test {

TestScreenManagerDelegate::TestScreenManagerDelegate(aura::TestScreen* screen)
    : screen_(screen) {
}

TestScreenManagerDelegate::~TestScreenManagerDelegate() {
}

void TestScreenManagerDelegate::SetWorkAreaInsets(const gfx::Insets& insets) {
  screen_->SetWorkAreaInsets(insets);
}

}  // namespace test
}  // namespace athena
