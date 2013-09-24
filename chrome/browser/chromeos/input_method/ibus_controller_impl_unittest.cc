// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"
#include "chromeos/ime/input_method_property.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

// A mock class for testing AddObserver() and RemoveObserver() methods
// in IBusControllerImpl.
class TestIBusController : public IBusControllerImpl {
 public:
  TestIBusController() {
  }

  virtual ~TestIBusController() {
  }

  bool HasObservers() const {
    return observers_.might_have_observers();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestIBusController);
};

class TestObserver : public IBusController::Observer {
 public:
  // IBusController::Observer overrides:
  virtual void PropertyChanged() OVERRIDE {}
};
}  // namespace

TEST(IBusControllerImplTest, TestAddRemoveObserver) {
  IBusBridge::Initialize();
  {
    TestIBusController controller;
    TestObserver observer1;
    TestObserver observer2;
    TestObserver observer3;
    EXPECT_FALSE(controller.HasObservers());
    controller.AddObserver(&observer1);
    EXPECT_TRUE(controller.HasObservers());
    controller.AddObserver(&observer2);
    EXPECT_TRUE(controller.HasObservers());
    controller.RemoveObserver(&observer3);  // nop
    EXPECT_TRUE(controller.HasObservers());
    controller.RemoveObserver(&observer1);
    EXPECT_TRUE(controller.HasObservers());
    controller.RemoveObserver(&observer1);  // nop
    EXPECT_TRUE(controller.HasObservers());
    controller.RemoveObserver(&observer2);
    EXPECT_FALSE(controller.HasObservers());
  }
  IBusBridge::Shutdown();
}

TEST(IBusControllerImplTest, TestGetCurrentProperties) {
  IBusBridge::Initialize();
  {
    IBusControllerImpl controller;
    EXPECT_EQ(0U, controller.GetCurrentProperties().size());
  }
  IBusBridge::Shutdown();
}

}  // namespace input_method
}  // namespace chromeos
