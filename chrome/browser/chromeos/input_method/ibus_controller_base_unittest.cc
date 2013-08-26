// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

// A mock class for testing SetInputMethodConfig(), AddObserver(), and
// RemoveObserver() methods in IBusControllerBase.
class TestIBusController : public IBusControllerBase {
 public:
  TestIBusController() {
  }
  virtual ~TestIBusController() {
  }

  // IBusController overrides:
  virtual void ClearProperties() OVERRIDE {}
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE {
    return true;
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

class IBusControllerBaseTest : public testing::Test {
 public:
  virtual void SetUp() {
    controller_.reset(new TestIBusController);
  }
  virtual void TearDown() {
    controller_.reset();
  }

 protected:
  scoped_ptr<TestIBusController> controller_;
};

}  // namespace

TEST_F(IBusControllerBaseTest, TestAddRemoveObserver) {
  TestObserver observer1;
  TestObserver observer2;
  TestObserver observer3;
  EXPECT_FALSE(controller_->HasObservers());
  controller_->AddObserver(&observer1);
  EXPECT_TRUE(controller_->HasObservers());
  controller_->AddObserver(&observer2);
  EXPECT_TRUE(controller_->HasObservers());
  controller_->RemoveObserver(&observer3);  // nop
  EXPECT_TRUE(controller_->HasObservers());
  controller_->RemoveObserver(&observer1);
  EXPECT_TRUE(controller_->HasObservers());
  controller_->RemoveObserver(&observer1);  // nop
  EXPECT_TRUE(controller_->HasObservers());
  controller_->RemoveObserver(&observer2);
  EXPECT_FALSE(controller_->HasObservers());
}

TEST_F(IBusControllerBaseTest, TestGetCurrentProperties) {
  EXPECT_EQ(0U, controller_->GetCurrentProperties().size());
}

}  // namespace input_method
}  // namespace chromeos
