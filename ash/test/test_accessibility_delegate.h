// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_ACCESSIBILITY_DELEGATE_H_
#define ASH_TEST_TEST_ACCESSIBILITY_DELEGATE_H_

#include "ash/default_accessibility_delegate.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace test {

class TestAccessibilityDelegate : public DefaultAccessibilityDelegate {
 public:
  TestAccessibilityDelegate() {}
  ~TestAccessibilityDelegate() override {}

  // AccessibilityDelegate:
  void PlayEarcon(int sound_key) override;

  int GetPlayedEarconAndReset();

 private:
  int sound_key_ = -1;

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_ACCESSIBILITY_DELEGATE_H_
