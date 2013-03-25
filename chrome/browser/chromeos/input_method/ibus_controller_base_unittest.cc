// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"
#include "chromeos/ime/input_method_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

// A mock class for testing SetInputMethodConfig(), AddObserver(), and
// RemoveObserver() methods in IBusControllerBase.
class TestIBusController : public IBusControllerBase {
 public:
  TestIBusController()
      : set_input_method_config_internal_count_(0),
        set_input_method_config_internal_return_(true) {
  }
  virtual ~TestIBusController() {
  }

  // IBusController overrides:
  virtual bool ChangeInputMethod(const std::string& id) OVERRIDE {
    return true;
  }
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE {
    return true;
  }

  size_t GetObserverCount() const {
    return observers_.size();
  }

  int set_input_method_config_internal_count() const {
    return set_input_method_config_internal_count_;
  }
  void set_set_input_method_config_internal_return(bool new_value) {
    set_input_method_config_internal_return_ = new_value;
  }
  const ConfigKeyType& last_key() const { return last_key_; }

 protected:
  virtual bool SetInputMethodConfigInternal(
      const ConfigKeyType& key,
      const InputMethodConfigValue& value) OVERRIDE {
    ++set_input_method_config_internal_count_;
    last_key_ = key;
    return set_input_method_config_internal_return_;
  }

 private:
  int set_input_method_config_internal_count_;
  bool set_input_method_config_internal_return_;
  ConfigKeyType last_key_;

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

TEST_F(IBusControllerBaseTest, TestSetInputMethodConfig) {
  InputMethodConfigValue value_set;
  InputMethodConfigValue value_get;
  EXPECT_FALSE(controller_->GetInputMethodConfigForTesting("section",
                                                           "name",
                                                           &value_get));
  EXPECT_EQ(0, controller_->set_input_method_config_internal_count());

  // Set a value.
  value_set.type = InputMethodConfigValue::kValueTypeInt;
  value_set.int_value = 12345;
  EXPECT_TRUE(controller_->SetInputMethodConfig("section",
                                                "name",
                                                value_set));
  EXPECT_EQ(1, controller_->set_input_method_config_internal_count());
  EXPECT_EQ("section", controller_->last_key().first);
  EXPECT_EQ("name", controller_->last_key().second);

  // Get the value.
  EXPECT_TRUE(controller_->GetInputMethodConfigForTesting("section",
                                                          "name",
                                                          &value_get));
  EXPECT_EQ(value_set.type, value_get.type);
  EXPECT_EQ(value_set.int_value, value_get.int_value);

  // Set another value.
  value_set.type = InputMethodConfigValue::kValueTypeBool;
  value_set.bool_value = true;
  EXPECT_TRUE(controller_->SetInputMethodConfig("section/2",
                                                "name2",
                                                value_set));
  EXPECT_EQ(2, controller_->set_input_method_config_internal_count());
  EXPECT_EQ("section/2", controller_->last_key().first);
  EXPECT_EQ("name2", controller_->last_key().second);

  // Overwrite the first value.
  value_set.type = InputMethodConfigValue::kValueTypeInt;
  value_set.int_value = 54321;
  EXPECT_TRUE(controller_->SetInputMethodConfig("section",
                                                "name",
                                                value_set));
  EXPECT_EQ(3, controller_->set_input_method_config_internal_count());
  EXPECT_EQ("section", controller_->last_key().first);
  EXPECT_EQ("name", controller_->last_key().second);

  // Get the value.
  EXPECT_TRUE(controller_->GetInputMethodConfigForTesting("section",
                                                          "name",
                                                          &value_get));
  EXPECT_EQ(value_set.type, value_get.type);
  EXPECT_EQ(value_set.int_value, value_get.int_value);

  // Get a non existent value.
  EXPECT_FALSE(controller_->GetInputMethodConfigForTesting("sectionX",
                                                           "name",
                                                           &value_get));
  EXPECT_FALSE(controller_->GetInputMethodConfigForTesting("section",
                                                           "nameX",
                                                           &value_get));
}

TEST_F(IBusControllerBaseTest, TestSetInputMethodConfigInternal) {
  InputMethodConfigValue value_set;
  InputMethodConfigValue value_get;
  // Set a value. In this case, SetInputMethodConfigInternal returns false.
  controller_->set_set_input_method_config_internal_return(false);
  value_set.type = InputMethodConfigValue::kValueTypeInt;
  value_set.int_value = 12345;
  EXPECT_FALSE(controller_->SetInputMethodConfig("section",
                                                 "name",
                                                 value_set));
  EXPECT_EQ(1, controller_->set_input_method_config_internal_count());

  // Getting the value should fail.
  EXPECT_FALSE(controller_->GetInputMethodConfigForTesting("section",
                                                           "name",
                                                           &value_get));
}

TEST_F(IBusControllerBaseTest, TestAddRemoveObserver) {
  TestObserver observer1;
  TestObserver observer2;
  TestObserver observer3;
  EXPECT_EQ(0U, controller_->GetObserverCount());
  controller_->AddObserver(&observer1);
  EXPECT_EQ(1U, controller_->GetObserverCount());
  controller_->AddObserver(&observer2);
  EXPECT_EQ(2U, controller_->GetObserverCount());
  controller_->RemoveObserver(&observer3);  // nop
  EXPECT_EQ(2U, controller_->GetObserverCount());
  controller_->RemoveObserver(&observer1);
  EXPECT_EQ(1U, controller_->GetObserverCount());
  controller_->RemoveObserver(&observer1);  // nop
  EXPECT_EQ(1U, controller_->GetObserverCount());
  controller_->RemoveObserver(&observer2);
  EXPECT_EQ(0U, controller_->GetObserverCount());
}

TEST_F(IBusControllerBaseTest, TestGetCurrentProperties) {
  EXPECT_EQ(0U, controller_->GetCurrentProperties().size());
}

}  // namespace input_method
}  // namespace chromeos
