// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_TEST_SCREEN_MANAGER_DELEGATE_H_
#define ATHENA_TEST_TEST_SCREEN_MANAGER_DELEGATE_H_

#include "athena/screen/public/screen_manager_delegate.h"
#include "base/macros.h"

namespace aura {
class TestScreen;
}

namespace athena {
namespace test {

class TestScreenManagerDelegate : public ScreenManagerDelegate {
 public:
  explicit TestScreenManagerDelegate(aura::TestScreen* screen);
  virtual ~TestScreenManagerDelegate();

 private:
  // ScreenManagerDelegate:
  virtual void SetWorkAreaInsets(const gfx::Insets& insets) OVERRIDE;

  // Not owned.
  aura::TestScreen* screen_;

  DISALLOW_COPY_AND_ASSIGN(TestScreenManagerDelegate);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_TEST_SCREEN_MANAGER_DELEGATE_H_
