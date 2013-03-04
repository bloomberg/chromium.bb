// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/input_method/mock_candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/mock_ibus_controller.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_delegate.h"
#include "chrome/browser/chromeos/input_method/mock_xkeyboard.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"
#include "chromeos/ime/mock_ibus_daemon_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/text_input_test_support.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace chromeos {

extern const char* kExtensionImePrefix;

namespace input_method {
namespace {

class InputMethodManagerImplTest :  public testing::Test {
 public:
  InputMethodManagerImplTest()
      : delegate_(NULL),
        controller_(NULL),
        candidate_window_controller_(NULL),
        xkeyboard_(NULL) {
  }
  virtual ~InputMethodManagerImplTest() {}

  virtual void SetUp() OVERRIDE {
    mock_ibus_daemon_controller_ = new chromeos::MockIBusDaemonController();
    chromeos::IBusDaemonController::InitializeForTesting(
        mock_ibus_daemon_controller_);
    mock_dbus_thread_manager_ =
        new chromeos::MockDBusThreadManagerWithoutGMock();
    chromeos::DBusThreadManager::InitializeForTesting(
        mock_dbus_thread_manager_);
    mock_ibus_client_ = mock_dbus_thread_manager_->mock_ibus_client();
    mock_ibus_input_context_client_ =
        mock_dbus_thread_manager_->mock_ibus_input_context_client();
    delegate_ = new MockInputMethodDelegate();
    manager_.reset(new InputMethodManagerImpl(
        scoped_ptr<InputMethodDelegate>(delegate_)));
    controller_ = new MockIBusController;
    manager_->SetIBusControllerForTesting(controller_);
    candidate_window_controller_ = new MockCandidateWindowController;
    manager_->SetCandidateWindowControllerForTesting(
        candidate_window_controller_);
    xkeyboard_ = new MockXKeyboard;
    manager_->SetXKeyboardForTesting(xkeyboard_);
  }

  virtual void TearDown() OVERRIDE {
    delegate_ = NULL;
    controller_ = NULL;
    candidate_window_controller_ = NULL;
    xkeyboard_ = NULL;
    manager_.reset();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::IBusDaemonController::Shutdown();
  }

 protected:
  scoped_ptr<InputMethodManagerImpl> manager_;
  MockInputMethodDelegate* delegate_;
  MockIBusController* controller_;
  MockCandidateWindowController* candidate_window_controller_;
  MockIBusDaemonController* mock_ibus_daemon_controller_;
  MockIBusInputContextClient* mock_ibus_input_context_client_;
  MockIBusClient* mock_ibus_client_;
  MockDBusThreadManagerWithoutGMock* mock_dbus_thread_manager_;
  MockXKeyboard* xkeyboard_;
  MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerImplTest);
};

class TestObserver : public InputMethodManager::Observer {
 public:
  TestObserver()
      : input_method_changed_count_(0),
        input_method_property_changed_count_(0),
        last_show_message_(false) {
  }
  virtual ~TestObserver() {}

  virtual void InputMethodChanged(InputMethodManager* manager,
                                  bool show_message) OVERRIDE {
    ++input_method_changed_count_;
    last_show_message_ = show_message;
  }
  virtual void InputMethodPropertyChanged(
      InputMethodManager* manager) OVERRIDE {
    ++input_method_property_changed_count_;
  }

  int input_method_changed_count_;
  int input_method_property_changed_count_;
  bool last_show_message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestCandidateWindowObserver
    : public InputMethodManager::CandidateWindowObserver {
 public:
  TestCandidateWindowObserver()
      : candidate_window_opened_count_(0),
        candidate_window_closed_count_(0) {
  }
  virtual ~TestCandidateWindowObserver() {}

  virtual void CandidateWindowOpened(InputMethodManager* manager) OVERRIDE {
    ++candidate_window_opened_count_;
  }
  virtual void CandidateWindowClosed(InputMethodManager* manager) OVERRIDE {
    ++candidate_window_closed_count_;
  }

  int candidate_window_opened_count_;
  int candidate_window_closed_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCandidateWindowObserver);
};

}  // namespace

TEST_F(InputMethodManagerImplTest, TestGetXKeyboard) {
  EXPECT_TRUE(manager_->GetXKeyboard());
  EXPECT_EQ(xkeyboard_, manager_->GetXKeyboard());
}

TEST_F(InputMethodManagerImplTest, TestCandidateWindowObserver) {
  TestCandidateWindowObserver observer;
  candidate_window_controller_->NotifyCandidateWindowOpened();  // nop
  candidate_window_controller_->NotifyCandidateWindowClosed();  // nop
  manager_->AddCandidateWindowObserver(&observer);
  candidate_window_controller_->NotifyCandidateWindowOpened();
  EXPECT_EQ(1, observer.candidate_window_opened_count_);
  candidate_window_controller_->NotifyCandidateWindowClosed();
  EXPECT_EQ(1, observer.candidate_window_closed_count_);
  candidate_window_controller_->NotifyCandidateWindowOpened();
  EXPECT_EQ(2, observer.candidate_window_opened_count_);
  candidate_window_controller_->NotifyCandidateWindowClosed();
  EXPECT_EQ(2, observer.candidate_window_closed_count_);
  manager_->RemoveCandidateWindowObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestObserver) {
  // For http://crbug.com/19655#c11 - (3). browser_state_monitor_unittest.cc is
  // also for the scenario.
  TestObserver observer;
  manager_->AddObserver(&observer);
  EXPECT_EQ(0, observer.input_method_changed_count_);
  manager_->EnableLayouts("en-US", "xkb:us::eng");
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(1, observer.input_method_property_changed_count_);
  manager_->ChangeInputMethod("xkb:us:dvorak:eng");
  EXPECT_FALSE(observer.last_show_message_);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(2, observer.input_method_property_changed_count_);
  manager_->ChangeInputMethod("xkb:us:dvorak:eng");
  EXPECT_FALSE(observer.last_show_message_);
  // The observer is always notified even when the same input method ID is
  // passed to ChangeInputMethod() more than twice.
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(3, observer.input_method_property_changed_count_);

  controller_->NotifyPropertyChangedForTesting();
  EXPECT_EQ(4, observer.input_method_property_changed_count_);
  controller_->NotifyPropertyChangedForTesting();
  EXPECT_EQ(5, observer.input_method_property_changed_count_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestGetSupportedInputMethods) {
  scoped_ptr<InputMethodDescriptors> methods(
      manager_->GetSupportedInputMethods());
  ASSERT_TRUE(methods.get());
  // Try to find random 4-5 layuts and IMEs to make sure the returned list is
  // correct.
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId("mozc");
  EXPECT_NE(methods->end(),
            std::find(methods->begin(), methods->end(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "mozc-chewing");
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:us::eng");
  EXPECT_NE(methods->end(),
            std::find(methods->begin(), methods->end(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:us:dvorak:eng");
  EXPECT_NE(methods->end(),
            std::find(methods->begin(), methods->end(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:fr::fra");
  EXPECT_NE(methods->end(),
            std::find(methods->begin(), methods->end(), *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestEnableLayouts) {
  // Currently 5 keyboard layouts are supported for en-US, and 1 for ja. See
  // ibus_input_method.txt.
  manager_->EnableLayouts("en-US", "");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  {
    // For http://crbug.com/19655#c11 - (1)
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    const InputMethodDescriptor* id_to_find =
        manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
            "english-m");  // The "English Mystery" IME.
    EXPECT_EQ(methods->end(),
              std::find(methods->begin(), methods->end(), *id_to_find));
  }
  // For http://crbug.com/19655#c11 - (2)
  EXPECT_EQ(0, mock_ibus_daemon_controller_->start_count());

  // For http://crbug.com/19655#c11 - (5)
  // The hardware keyboard layout "xkb:us::eng" is always active, hence 2U.
  manager_->EnableLayouts("ja", "");  // Japanese
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(0, mock_ibus_daemon_controller_->start_count());
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsNonUsHardwareKeyboard) {
  // The physical layout is French.
  delegate_->set_hardware_keyboard_layout("xkb:fr::fra");
  manager_->EnableLayouts("en-US", "");
  EXPECT_EQ(6U, manager_->GetNumActiveInputMethods());  // 5 + French
  // The physical layout is Japanese.
  delegate_->set_hardware_keyboard_layout("xkb:jp::jpn");
  manager_->EnableLayouts("ja", "");
  // "xkb:us::eng" is not needed, hence 1.
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestActiveInputMethods) {
  manager_->EnableLayouts("ko", "");  // Korean
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  scoped_ptr<InputMethodDescriptors> methods(
      manager_->GetActiveInputMethods());
  ASSERT_TRUE(methods.get());
  EXPECT_EQ(2U, methods->size());
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          "xkb:us::eng");
  EXPECT_NE(methods->end(),
            std::find(methods->begin(), methods->end(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:kr:kr104:kor");
  EXPECT_NE(methods->end(),
            std::find(methods->begin(), methods->end(), *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestSetInputMethodConfig) {
  InputMethodConfigValue config;
  config.type = InputMethodConfigValue::kValueTypeString;
  config.string_value = "string";
  EXPECT_EQ(0, controller_->set_input_method_config_internal_count_);
  EXPECT_TRUE(manager_->SetInputMethodConfig("section", "name", config));
  EXPECT_EQ(1, controller_->set_input_method_config_internal_count_);
  EXPECT_EQ("section",
            controller_->set_input_method_config_internal_key_.first);
  EXPECT_EQ("name",
            controller_->set_input_method_config_internal_key_.second);
  EXPECT_EQ(config.type,
            controller_->set_input_method_config_internal_value_.type);
  EXPECT_EQ(config.string_value,
            controller_->set_input_method_config_internal_value_.string_value);

  // SetInputMethodConfig should be no-op in STATE_TERMINATING.
  manager_->SetState(InputMethodManager::STATE_TERMINATING);
  EXPECT_FALSE(manager_->SetInputMethodConfig("section", "name", config));
  EXPECT_EQ(1, controller_->set_input_method_config_internal_count_);
}

TEST_F(InputMethodManagerImplTest, TestEnableTwoLayouts) {
  // For http://crbug.com/19655#c11 - (8), step 6.
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("xkb:us:colemak:eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  // Since all the IDs added avobe are keyboard layouts, Start() should not be
  // called.
  EXPECT_EQ(0, mock_ibus_daemon_controller_->start_count());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0],  // colemak
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableThreeLayouts) {
  // For http://crbug.com/19655#c11 - (9).
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("xkb:us:colemak:eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  // Switch to Dvorak.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0],  // US Qwerty
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme) {
  // For http://crbug.com/19655#c11 - (10).
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("mozc");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1, mock_ibus_daemon_controller_->start_count());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Switch to Mozc
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  // Disable Mozc.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Currently, to work around  a crash issue at crosbug.com/27051,
  // controller_->Stop(); is NOT called when all IMEs are disabled.
  EXPECT_EQ(0, mock_ibus_daemon_controller_->stop_count());

  // However, IME should always be stopped on shutdown.
  manager_->SetState(InputMethodManager::STATE_TERMINATING);
  EXPECT_EQ(1, mock_ibus_daemon_controller_->stop_count());
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme2) {
  // For http://crbug.com/19655#c11 - (11).
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("mozc");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1, mock_ibus_daemon_controller_->start_count());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[0],  // Mozc
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableImes) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("mozc-chewing");
  ids.push_back("mozc-dv");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1, mock_ibus_daemon_controller_->start_count());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableUnknownIds) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:tl::tlh");  // Klingon, which is not supported.
  ids.push_back("unknown-super-cool-ime");
  EXPECT_FALSE(manager_->EnableInputMethods(ids));

  // TODO(yusukes): Should we fall back to the hardware keyboard layout in this
  // case?
  EXPECT_EQ(0, observer.input_method_changed_count_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsThenLock) {
  // For http://crbug.com/19655#c11 - (14).
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Switch to Dvorak.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Lock screen
  manager_->SetState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[1],  // still Dvorak
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Switch back to Qwerty.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Unlock screen. The original state, Dvorak, is restored.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndImeThenLock) {
  // For http://crbug.com/19655#c11 - (15).
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("mozc-dv");
  ids.push_back("mozc-chewing");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Switch to Mozc.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Lock screen
  manager_->SetState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());  // Qwerty+Dvorak.
  EXPECT_EQ("xkb:us:dvorak:eng",
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // controller_->Stop() should never be called when the screen is locked even
  // after crosbug.com/27051 is fixed.
  EXPECT_EQ(0, mock_ibus_daemon_controller_->stop_count());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us::eng",  // The hardware keyboard layout.
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Unlock screen. The original state, mozc-dv, is restored.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());  // Dvorak and 2 IMEs.
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestXkbSetting) {
  // For http://crbug.com/19655#c11 - (8), step 7-11.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("xkb:us:colemak:eng");
  ids.push_back("mozc-jp");
  ids.push_back("mozc");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(4U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  // See input_methods.txt for an expected XKB layout name.
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(3, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(4, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(5, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(6, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestActivateInputMethodProperty) {
  manager_->ActivateInputMethodProperty("key");
  EXPECT_EQ(1, controller_->activate_input_method_property_count_);
  EXPECT_EQ("key", controller_->activate_input_method_property_key_);
  manager_->ActivateInputMethodProperty("key2");
  EXPECT_EQ(2, controller_->activate_input_method_property_count_);
  EXPECT_EQ("key2", controller_->activate_input_method_property_key_);
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodProperties) {
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back("mozc");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());
  manager_->ChangeInputMethod("mozc");

  InputMethodPropertyList current_property_list;
  current_property_list.push_back(InputMethodProperty("key",
                                                      "label",
                                                      false,
                                                      false));
  controller_->SetCurrentPropertiesForTesting(current_property_list);
  controller_->NotifyPropertyChangedForTesting();

  ASSERT_EQ(1U, manager_->GetCurrentInputMethodProperties().size());
  EXPECT_EQ("key", manager_->GetCurrentInputMethodProperties().at(0).key);

  manager_->ChangeInputMethod("xkb:us::eng");
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  // Delayed asynchronous property update signal from the Mozc IME.
  controller_->NotifyPropertyChangedForTesting();
  // When XKB layout is in use, GetCurrentInputMethodProperties() should always
  // return an empty list.
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodPropertiesTwoImes) {
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("mozc");  // Japanese
  ids.push_back("mozc-chewing");  // T-Chinese
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  InputMethodPropertyList current_property_list;
  current_property_list.push_back(InputMethodProperty("key-mozc",
                                                      "label",
                                                      false,
                                                      false));
  controller_->SetCurrentPropertiesForTesting(current_property_list);
  controller_->NotifyPropertyChangedForTesting();

  ASSERT_EQ(1U, manager_->GetCurrentInputMethodProperties().size());
  EXPECT_EQ("key-mozc", manager_->GetCurrentInputMethodProperties().at(0).key);

  manager_->ChangeInputMethod("mozc-chewing");
  // Since the IME is changed, the property for mozc Japanese should be hidden.
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  // Asynchronous property update signal from mozc-chewing.
  current_property_list.clear();
  current_property_list.push_back(InputMethodProperty("key-chewing",
                                                      "label",
                                                      false,
                                                      false));
  controller_->SetCurrentPropertiesForTesting(current_property_list);
  controller_->NotifyPropertyChangedForTesting();
  ASSERT_EQ(1U, manager_->GetCurrentInputMethodProperties().size());
  EXPECT_EQ("key-chewing",
            manager_->GetCurrentInputMethodProperties().at(0).key);
}

TEST_F(InputMethodManagerImplTest, TestNextInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  // For http://crbug.com/19655#c11 - (1)
  manager_->EnableLayouts("en-US", "xkb:us::eng");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:dvorak:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:colemak:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestPreviousInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->EnableLayouts("en-US", "xkb:us::eng");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithUsLayouts) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->EnableLayouts("en-US", "xkb:us::eng");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Henkan, Muhenkan, ZenkakuHankaku should be ignored when no Japanese IMEs
  // and keyboards are enabled.
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, ui::EF_NONE)));
  EXPECT_FALSE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Do the same tests for Korean.
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithJpLayout) {
  // Enable "xkb:jp::jpn" and press Muhenkan/ZenkakuHankaku.
  manager_->EnableLayouts("ja", "xkb:us::eng");
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithKoLayout) {
  // Do the same tests for Korean.
  manager_->EnableLayouts("ko", "xkb:us::eng");
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithJpIme) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:jp::jpn");
  ids.push_back("mozc-jp");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("mozc-jp", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, ui::EF_NONE)));
  EXPECT_EQ("mozc-jp", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, ui::EF_NONE)));
  EXPECT_EQ("mozc-jp", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);

  // Add Dvorak.
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("mozc-jp", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithKoIme) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:kr:kr104:kor");
  ids.push_back("mozc-hangul");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("mozc-hangul", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);

  // Add Dvorak.
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("mozc-hangul", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, ui::EF_NONE)));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("kr(kr104)", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestAddRemoveExtensionInputMethods) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(0, mock_ibus_daemon_controller_->start_count());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0],
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Add two Extension IMEs.
  std::vector<std::string> layouts;
  layouts.push_back("us");
  manager_->AddInputMethodExtension(
      std::string(kExtensionImePrefix) + "deadbeef",
      "deadbeef input method",
      layouts,
      "en-US",
      NULL);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());

  // should be started.
  EXPECT_EQ(1, mock_ibus_daemon_controller_->start_count());
  {
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    ASSERT_EQ(2U, methods->size());
    EXPECT_EQ(std::string(kExtensionImePrefix) + "deadbeef",
              // Ext IMEs should be at the end of the list.
              methods->at(1).id());
  }
  manager_->AddInputMethodExtension(
      std::string(kExtensionImePrefix) + "cafebabe",
      "cafebabe input method",
      layouts,
      "en-US",
      NULL);
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  {
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    ASSERT_EQ(3U, methods->size());
    EXPECT_EQ(std::string(kExtensionImePrefix) + "deadbeef",
              // Ext IMEs should be at the end of the list.
              methods->at(1).id());
  }

  // Remove them.
  manager_->RemoveInputMethodExtension(
      std::string(kExtensionImePrefix) + "deadbeef");
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  manager_->RemoveInputMethodExtension(
      std::string(kExtensionImePrefix) + "cafebabe");
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  // Currently, to work around  a crash issue at crosbug.com/27051,
  // controller_->Stop(); is NOT called when all (extension) IMEs are disabled.
  EXPECT_EQ(0, mock_ibus_daemon_controller_->stop_count());

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestAddExtensionInputThenLockScreen) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Add an Extension IME.
  std::vector<std::string> layouts;
  layouts.push_back("us(dvorak)");
  manager_->AddInputMethodExtension(
      std::string(kExtensionImePrefix) + "deadbeef",
      "deadbeef input method",
      layouts,
      "en-US",
      NULL);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);

  // Switch to the IME.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(std::string(kExtensionImePrefix) + "deadbeef",
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Lock the screen. This is for crosbug.com/27049.
  manager_->SetState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());  // Qwerty. No Ext. IME
  EXPECT_EQ("xkb:us::eng",
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_EQ(0, mock_ibus_daemon_controller_->stop_count());

  // Unlock the screen.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(std::string(kExtensionImePrefix) + "deadbeef",
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  {
    // This is for crosbug.com/27052.
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    ASSERT_EQ(2U, methods->size());
    EXPECT_EQ(std::string(kExtensionImePrefix) + "deadbeef",
              // Ext. IMEs should be at the end of the list.
              methods->at(1).id());
  }
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestReset) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back("mozc");
  EXPECT_TRUE(manager_->EnableInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, mock_ibus_input_context_client_->reset_call_count());
  manager_->ChangeInputMethod("mozc");
  EXPECT_EQ(1, controller_->change_input_method_count_);
  EXPECT_EQ("mozc", controller_->change_input_method_id_);
  EXPECT_EQ(1, mock_ibus_input_context_client_->reset_call_count());
  manager_->ChangeInputMethod("xkb:us::eng");
  EXPECT_EQ(2, controller_->change_input_method_count_);
  EXPECT_EQ("mozc", controller_->change_input_method_id_);
  EXPECT_EQ(1, mock_ibus_input_context_client_->reset_call_count());
}

}  // namespace input_method
}  // namespace chromeos
