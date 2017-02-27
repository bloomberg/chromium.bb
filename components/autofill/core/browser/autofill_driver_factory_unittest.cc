// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_factory.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD0(HideAutofillPopup, void());
};

// Just a stub AutofillDriver implementation which announces its construction
// and desctruction by updating the passed |instance_counter|. It also records
// when "user gesture observed" was signalled to it.
class CountingAutofillDriver : public TestAutofillDriver {
 public:
  CountingAutofillDriver(int* instance_counter)
      : instance_counter_(instance_counter) {
    ++*instance_counter;
  }

  ~CountingAutofillDriver() override { --*instance_counter_; }

  // Note that EXPECT_CALL cannot be used here, because creation and
  // notification of the same driver might be done by a single AddForKey call
  // from the test. Therefore tracking the "gesture_observed" flag is done
  // explicitly here.
  void NotifyFirstUserGestureObservedInTab() override {
    gesture_observed_ = true;
  }

  bool gesture_observed() { return gesture_observed_; }

 private:
  int* const instance_counter_;
  bool gesture_observed_ = false;

  DISALLOW_COPY_AND_ASSIGN(CountingAutofillDriver);
};

// Code-wise this class is identitcal to AutofillDriverFactory, but exposes the
// protected API to the test. Do not modify any of the methods, only include
// "using" declarations to make the AutofillDriverFactory methods public.
class PublicAutofillDriverFactory : public AutofillDriverFactory {
 public:
  explicit PublicAutofillDriverFactory(AutofillClient* client)
      : AutofillDriverFactory(client) {}

  ~PublicAutofillDriverFactory() {}

  using AutofillDriverFactory::AddForKey;
  using AutofillDriverFactory::DeleteForKey;
};

// Wrapper around an integer, checking that the integer is 0 on desctruction.
class CheckedInt {
 public:
  CheckedInt() {}

  ~CheckedInt() { EXPECT_EQ(0, val_); }

  int* val() { return &val_; }

 private:
  int val_ = 0;
};

}  // namespace

class AutofillDriverFactoryTest : public testing::Test {
 public:
  AutofillDriverFactoryTest() : factory_(&client_) {}

  ~AutofillDriverFactoryTest() override {}

  // AutofillDriverFactory stores drivers in a map with keys, which are void*
  // pointers. The factory never dereferences them, so their value does not
  // matter. This is a handy function to create such pointers from integer
  // constants.
  void* KeyFrom(int x) { return reinterpret_cast<void*>(x); }

  // Convenience accessor with a cast to CountingAutofillDriver.
  CountingAutofillDriver* GetDriver(void* key) {
    return static_cast<CountingAutofillDriver*>(factory_.DriverForKey(key));
  }

  std::unique_ptr<AutofillDriver> CreateDriver() {
    ++drivers_created_;
    return base::MakeUnique<CountingAutofillDriver>(instance_counter_.val());
  }

  base::Callback<std::unique_ptr<AutofillDriver>()> CreateDriverCallback() {
    return base::Bind(&AutofillDriverFactoryTest::CreateDriver,
                      base::Unretained(this));
  }

 protected:
  base::MessageLoop message_loop_;  // For TestAutofillDriver.

  MockAutofillClient client_;

  CheckedInt instance_counter_;

  PublicAutofillDriverFactory factory_;

  // How many AutofillDriver instances were created with CreateDriver().
  int drivers_created_ = 0;
};

TEST_F(AutofillDriverFactoryTest, DriverForKey_NoKey) {
  EXPECT_FALSE(factory_.DriverForKey(nullptr));
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
}

TEST_F(AutofillDriverFactoryTest, DriverForKey_OneKey) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_FALSE(factory_.DriverForKey(nullptr));
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
}

TEST_F(AutofillDriverFactoryTest, DriverForKey_TwoKeys) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_FALSE(factory_.DriverForKey(nullptr));
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, *instance_counter_.val());

  factory_.AddForKey(nullptr, CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(nullptr));
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(2, *instance_counter_.val());
}

TEST_F(AutofillDriverFactoryTest, AddForKey_Duplicated) {
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));

  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, drivers_created_);
  EXPECT_EQ(1, *instance_counter_.val());

  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, drivers_created_);
  EXPECT_EQ(1, *instance_counter_.val());
}

TEST_F(AutofillDriverFactoryTest, DeleteForKey) {
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(0, *instance_counter_.val());

  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(1, *instance_counter_.val());

  factory_.DeleteForKey(KeyFrom(1));
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(0, *instance_counter_.val());

  // Duplicated calls should raise no errors.
  factory_.DeleteForKey(KeyFrom(1));
  EXPECT_FALSE(factory_.DriverForKey(KeyFrom(1)));
  EXPECT_EQ(0, *instance_counter_.val());
}

TEST_F(AutofillDriverFactoryTest, NavigationFinished) {
  EXPECT_CALL(client_, HideAutofillPopup());
  factory_.NavigationFinished();
}

TEST_F(AutofillDriverFactoryTest, TabHidden) {
  EXPECT_CALL(client_, HideAutofillPopup());
  factory_.TabHidden();
}

// Without calling OnFirstUserGestureObserved on the factory, the factory will
// not call NotifyFirstUserGestureObservedInTab on a driver.
TEST_F(AutofillDriverFactoryTest, OnFirstUserGestureObserved_NotCalled) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_FALSE(GetDriver(KeyFrom(1))->gesture_observed());
}

// Call OnFirstUserGestureObserved on the factory with one driver. The factory
// will call NotifyFirstUserGestureObservedInTab on that driver.
TEST_F(AutofillDriverFactoryTest, OnFirstUserGestureObserved_CalledOld) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  factory_.OnFirstUserGestureObserved();
  EXPECT_TRUE(GetDriver(KeyFrom(1))->gesture_observed());
}

// Call OnFirstUserGestureObserved on the factory without drivers. Add a
// driver. The factory will call NotifyFirstUserGestureObservedInTab on that
// driver.
TEST_F(AutofillDriverFactoryTest, OnFirstUserGestureObserved_CalledNew) {
  factory_.OnFirstUserGestureObserved();
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  EXPECT_TRUE(GetDriver(KeyFrom(1))->gesture_observed());
}

// Combining the CalledOld and CalledNew test cases into one.
TEST_F(AutofillDriverFactoryTest, OnFirstUserGestureObserved_MultipleDrivers) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  factory_.OnFirstUserGestureObserved();
  EXPECT_TRUE(GetDriver(KeyFrom(1))->gesture_observed());

  factory_.AddForKey(KeyFrom(7), CreateDriverCallback());
  EXPECT_TRUE(GetDriver(KeyFrom(7))->gesture_observed());
}

// Call OnFirstUserGestureObserved on the factory with one driver. Simulate
// navigation to a different page. Add a driver. The factory will not call
// NotifyFirstUserGestureObservedInTab on that driver.
TEST_F(AutofillDriverFactoryTest, OnFirstUserGestureObserved_CalledNavigation) {
  factory_.AddForKey(KeyFrom(1), CreateDriverCallback());
  factory_.OnFirstUserGestureObserved();
  EXPECT_TRUE(GetDriver(KeyFrom(1))->gesture_observed());

  EXPECT_CALL(client_, HideAutofillPopup());
  factory_.NavigationFinished();

  // Adding a sub-frame
  factory_.AddForKey(KeyFrom(2), CreateDriverCallback());
  EXPECT_FALSE(GetDriver(KeyFrom(2))->gesture_observed());
}

}  // namespace autofill
