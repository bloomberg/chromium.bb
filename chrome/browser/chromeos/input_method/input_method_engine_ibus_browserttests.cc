// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/test/test_utils.h"
#include "dbus/mock_bus.h"

// TODO(nona): Remove gmock dependency once crbug.com/223061 is fixed.
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;
using testing::_;

namespace chromeos {
namespace input_method {
namespace {

const char kIdentityIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpIdentityIME";
const char kToUpperIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpToUpperIME";
const char kEchoBackIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpEchoBackIME";

// InputMethod extension should work on 1)normal extension, 2)normal extension
// in incognito mode 3)component extension.
enum TestType {
  kTestTypeNormal = 0,
  kTestTypeIncognito = 1,
  kTestTypeComponent = 2,
};

void OnRegisterComponent(const IBusComponent& ibus_component,
                         const IBusClient::RegisterComponentCallback& callback,
                         const IBusClient::ErrorCallback& error_callback) {
  callback.Run();
}

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

class KeyEventDoneCallback {
 public:
  explicit KeyEventDoneCallback(bool expected_argument)
      : expected_argument_(expected_argument),
        is_called_(false) {}
  ~KeyEventDoneCallback() {}

 public:
  void Run(bool consumed) {
    if (consumed == expected_argument_) {
      MessageLoop::current()->Quit();
      is_called_ = true;
    }
  }

  void WaitUntilCalled() {
    while (!is_called_)
      content::RunMessageLoop();
  }

 private:
  bool expected_argument_;
  bool is_called_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventDoneCallback);
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
                       BasicScenarioTest) {
  MockIBusClient* ibus_client = mock_dbus_thread_manager_->mock_ibus_client();
  ibus_client->set_register_component_handler(base::Bind(&OnRegisterComponent));

  // This will load "chrome/test/data/extensions/input_ime"
  ExtensionTestMessageListener ime_ready_listener("ReadyToUseImeEvent", false);
  ASSERT_TRUE(LoadExtensionWithType("input_ime", GetParam()));
  ASSERT_TRUE(ime_ready_listener.WaitUntilSatisfied());

  // The reason why not EXPECT_EQ is that extension will be reloaded in the
  // case of incognito mode switching. Thus registeration will be happend
  // multiple times. Calling at least once per engine is sufficient for IBus
  // component. Here, there is two engine, thus expectation is at least twice.
  EXPECT_LE(3, ibus_client->register_component_call_count());

  InputMethodDescriptors extension_imes;
  GetInputMethodManager()->GetInputMethodExtensions(&extension_imes);

  // Test IME has two input methods, thus InputMethodManager should have two
  // extension IME.
  // Note: Even extension is loaded by LoadExtensionAsComponent as above, the
  // IME does not managed by ComponentExtensionIMEManager or it's id won't start
  // with __comp__. The component extension IME is whitelisted and managed by
  // ComponentExtensionIMEManager, but its framework is same as normal extension
  // IME.
  EXPECT_EQ(3U, extension_imes.size());

  MockIBusEngineFactoryService* factory_service =
      mock_dbus_thread_manager_->mock_ibus_engine_factory_service();
  factory_service->CallCreateEngine(kEchoBackIMEID);

  MockIBusEngineService* engine_service =
      mock_dbus_thread_manager_->mock_ibus_engine_service();
  IBusEngineHandlerInterface* engine_handler = engine_service->GetEngine();
  ASSERT_TRUE(engine_handler);

  // onActivate event should be fired if Enable function is called.
  ExtensionTestMessageListener activated_listener("onActivate", false);
  engine_handler->Enable();
  ASSERT_TRUE(activated_listener.WaitUntilSatisfied());

  // onFocus event should be fired if FocusIn function is called.
  ExtensionTestMessageListener focus_listener("onFocus", false);;
  engine_handler->FocusIn();
  ASSERT_TRUE(focus_listener.WaitUntilSatisfied());

  // onKeyEvent should be fired if ProcessKeyEvent is called.
  KeyEventDoneCallback callback(false);  // EchoBackIME doesn't consume keys.
  ExtensionTestMessageListener keyevent_listener("onKeyEvent", false);
  engine_handler->ProcessKeyEvent(0x61,  // KeySym for 'a'.
                                  0x26,  // KeyCode for 'a'.
                                  0,  // No modifiers.
                                  base::Bind(&KeyEventDoneCallback::Run,
                                             base::Unretained(&callback)));
  ASSERT_TRUE(keyevent_listener.WaitUntilSatisfied());
  callback.WaitUntilCalled();

  // onSurroundingTextChange should be fired if SetSurroundingText is called.
  ExtensionTestMessageListener surrounding_text_listener(
      "onSurroundingTextChanged", false);
  engine_handler->SetSurroundingText("text",  // Surrounding text.
                                     0,  // focused position.
                                     1);  // anchor position.
  ASSERT_TRUE(surrounding_text_listener.WaitUntilSatisfied());

  // onMenuItemActivated should be fired if PropertyActivate is called.
  ExtensionTestMessageListener property_listener("onMenuItemActivated", false);
  engine_handler->PropertyActivate("property_name",
                                   ibus::IBUS_PROPERTY_STATE_CHECKED);
  ASSERT_TRUE(property_listener.WaitUntilSatisfied());

  // onBlur should be fired if FocusOut is called.
  ExtensionTestMessageListener blur_listener("onBlur", false);
  engine_handler->FocusOut();
  ASSERT_TRUE(blur_listener.WaitUntilSatisfied());

  // onDeactivated should be fired if Disable is called.
  ExtensionTestMessageListener disabled_listener("onDeactivated", false);
  engine_handler->Disable();
  ASSERT_TRUE(disabled_listener.WaitUntilSatisfied());
}

}  // namespace
}  // namespace input_method
}  // namespace chromeos
