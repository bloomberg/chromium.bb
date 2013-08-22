// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chromeos/ime/ibus_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(IBusControllerTest, TestCreate) {
  IBusBridge::Initialize();
  {
    scoped_ptr<IBusController> controller(IBusController::Create());
    EXPECT_TRUE(controller.get());
  }
  IBusBridge::Shutdown();
}

}  // namespace input_method
}  // namespace chromeos
