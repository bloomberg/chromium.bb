// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/test_accessibility_delegate.h"

namespace ash {

void TestAccessibilityDelegate::PlayEarcon(int sound_key) {
  sound_key_ = sound_key;
}

int TestAccessibilityDelegate::GetPlayedEarconAndReset() {
  int tmp = sound_key_;
  sound_key_ = -1;
  return tmp;
}

}  // namespace ash
