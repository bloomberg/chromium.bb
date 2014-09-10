// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/env/public/athena_env.h"

#include "athena/test/athena_test_base.h"
#include "base/bind.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace athena {

namespace {

class AthenaShutdownTest : public test::AthenaTestBase {
 public:
  AthenaShutdownTest() {}
  virtual ~AthenaShutdownTest() {}

  virtual void TearDown() {
    test::AthenaTestBase::TearDown();
    ASSERT_NE(
        gfx::Display::kInvalidDisplayID,
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaShutdownTest);
};

}  // namespace

// gfx::Screen should be accessible after shutdown.
TEST_F(AthenaShutdownTest, Shutdown) {
}

}  // namespace athena
