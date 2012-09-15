// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/extensions/power/power_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace extensions {
namespace power {

class PowerApiTest : public InProcessBrowserTest {
 public:
  PowerApiTest() {}
  virtual ~PowerApiTest() {}

  virtual void SetUp() OVERRIDE {
    chromeos::MockDBusThreadManager* mock_dbus_thread_manager =
        new chromeos::MockDBusThreadManager;

    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));
    chromeos::DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    power_client_ = mock_dbus_thread_manager->mock_power_manager_client();

    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    chromeos::DBusThreadManager::Shutdown();

    InProcessBrowserTest::TearDown();
  }

  scoped_refptr<Extension> CreateExtensionWithId(const std::string& id) {
    return extension_test_util::CreateExtensionWithID(
        extension_test_util::MakeId(id));
  }

  void RunFunctionAndExpectPass(UIThreadExtensionFunction* function,
                                Extension* extension) {
    function->set_extension(extension);
    function->set_has_callback(true);

    scoped_ptr<base::Value> result(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function, "[]", browser()));

    ASSERT_TRUE(result.get() != NULL);
    ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());
    bool boolean_value;
    result->GetAsBoolean(&boolean_value);
    EXPECT_EQ(boolean_value, true);
  }

  void RequestKeepAwake(scoped_refptr<Extension> extension) {
    scoped_refptr<RequestKeepAwakeFunction> function(
        new RequestKeepAwakeFunction);
    RunFunctionAndExpectPass(function.get(), extension);
  }

  void ReleaseKeepAwake(scoped_refptr<Extension> extension) {
    scoped_refptr<ReleaseKeepAwakeFunction> function(
        new ReleaseKeepAwakeFunction);
    RunFunctionAndExpectPass(function.get(), extension);
  }

 protected:
  chromeos::MockPowerManagerClient* power_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerApiTest);
};

IN_PROC_BROWSER_TEST_F(PowerApiTest, RequestAndRelease) {
  scoped_refptr<Extension> extension(CreateExtensionWithId("0"));

  EXPECT_CALL(*power_client_, RequestPowerStateOverrides(_,_,_,_)).Times(1);
  RequestKeepAwake(extension);

  EXPECT_CALL(*power_client_, CancelPowerStateOverrides(_)).Times(1);
  ReleaseKeepAwake(extension);
}

IN_PROC_BROWSER_TEST_F(PowerApiTest, RequestMultipleAndReleaseOne) {
  scoped_refptr<Extension> extension1(CreateExtensionWithId("1"));
  scoped_refptr<Extension> extension2(CreateExtensionWithId("2"));

  EXPECT_CALL(*power_client_, RequestPowerStateOverrides(_,_,_,_)).Times(1);
  RequestKeepAwake(extension1);
  RequestKeepAwake(extension2);
  RequestKeepAwake(extension1);

  EXPECT_CALL(*power_client_, CancelPowerStateOverrides(_)).Times(1);
  ReleaseKeepAwake(extension1);
}

IN_PROC_BROWSER_TEST_F(PowerApiTest, RequestOneAndReleaseMultiple) {
  scoped_refptr<Extension> extension(CreateExtensionWithId("3"));

  EXPECT_CALL(*power_client_, RequestPowerStateOverrides(_,_,_,_)).Times(1);
  RequestKeepAwake(extension);

  EXPECT_CALL(*power_client_, CancelPowerStateOverrides(_)).Times(1);
  ReleaseKeepAwake(extension);
  ReleaseKeepAwake(extension);
  ReleaseKeepAwake(extension);
}

IN_PROC_BROWSER_TEST_F(PowerApiTest, RequestMultipleAndReleaseAll) {
  scoped_refptr<Extension> extension1(CreateExtensionWithId("4"));
  scoped_refptr<Extension> extension2(CreateExtensionWithId("5"));
  scoped_refptr<Extension> extension3(CreateExtensionWithId("6"));

  EXPECT_CALL(*power_client_, RequestPowerStateOverrides(_,_,_,_)).Times(1);
  RequestKeepAwake(extension1);
  RequestKeepAwake(extension2);
  RequestKeepAwake(extension3);

  EXPECT_CALL(*power_client_, CancelPowerStateOverrides(_)).Times(1);
  ReleaseKeepAwake(extension3);
  ReleaseKeepAwake(extension1);
  ReleaseKeepAwake(extension2);
}

// TODO(rkc): Add another test to verify a Request->Release->Request scenario.

}  // namespace power
}  // namespace extensions
