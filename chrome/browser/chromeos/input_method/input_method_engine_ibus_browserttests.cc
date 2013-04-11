// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#include "dbus/mock_bus.h"

// TODO(nona): Remove gmock dependency once crbug.com/223061 is fixed.
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;
using testing::_;

namespace chromeos {
namespace {

// InputMethod extension should work on 1)normal extension, 2)normal extension
// in incognito mode 3)component extension.
enum TestType {
  kTestTypeNormal = 0,
  kTestTypeIncognito = 1,
  kTestTypeComponent = 2,
};

class InputMethodEngineIBusBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<TestType> {
 public:
  InputMethodEngineIBusBrowserTest()
      : ExtensionBrowserTest(),
        mock_dbus_thread_manager_(NULL) {}
  virtual ~InputMethodEngineIBusBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    mock_dbus_thread_manager_ = new MockDBusThreadManager();
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager_);
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    // Necessary for launching browser tests with MockDBusThreadManager.
    // TODO(nona): Remove this once crbug.com/223061 is fixed.
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_update_engine_client(),
                GetLastStatus())
        .Times(1)
        .WillOnce(Return(chromeos::MockUpdateEngineClient::Status()));

    // TODO(nona): Remove this once crbug.com/223061 is fixed.
    EXPECT_CALL(*mock_dbus_thread_manager_, InitIBusBus(_, _))
        .WillOnce(Invoke(this,
                         &InputMethodEngineIBusBrowserTest::OnInitIBusBus));
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    DBusThreadManager::Shutdown();
  }

 protected:
  bool LoadExtensionWithType(const std::string& extension_name,
                             TestType type) {
    switch (type) {
      case kTestTypeNormal:
        return LoadExtension(test_data_dir_.AppendASCII(extension_name));
      case kTestTypeIncognito:
        return LoadExtensionIncognito(
            test_data_dir_.AppendASCII(extension_name));
      case kTestTypeComponent:
        return LoadExtensionAsComponent(
            test_data_dir_.AppendASCII(extension_name));
    }
    NOTREACHED();
    return false;
  }

  void OnInitIBusBus(const std::string& ibus_address,
                     const base::Closure& closure) {
    dbus::Bus::Options options;
    mock_bus_ = new dbus::MockBus(options);
    EXPECT_CALL(*mock_dbus_thread_manager_, GetIBusBus())
        .WillRepeatedly(Return(mock_bus_));
  }

  MockDBusThreadManager* mock_dbus_thread_manager_;
  scoped_refptr<dbus::MockBus> mock_bus_;
};

INSTANTIATE_TEST_CASE_P(InputMethodEngineIBusBrowserTest,
                        InputMethodEngineIBusBrowserTest,
                        ::testing::Values(kTestTypeNormal));
INSTANTIATE_TEST_CASE_P(InputMethodEngineIBusIncognitoBrowserTest,
                        InputMethodEngineIBusBrowserTest,
                        ::testing::Values(kTestTypeIncognito));
INSTANTIATE_TEST_CASE_P(InputMethodEngineIBusComponentExtensionBrowserTest,
                        InputMethodEngineIBusBrowserTest,
                        ::testing::Values(kTestTypeComponent));

IN_PROC_BROWSER_TEST_P(InputMethodEngineIBusBrowserTest,
                       RegisterComponentTest) {
  MockIBusClient* ibus_client = mock_dbus_thread_manager_->mock_ibus_client();
  // This will load "chrome/test/data/extensions/input_ime"
  ASSERT_TRUE(LoadExtensionWithType("input_ime", GetParam()));
  // The reason why not EXPECT_EQ is that extension will be reloaded in the
  // case of incognito mode switching. Thus registeration will be happend
  // multiple times. Calling at least once is sufficient for IBus component.
  EXPECT_LE(1, ibus_client->register_component_call_count());
}

}  // namespace
}  // namespace chromeos
