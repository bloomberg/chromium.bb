// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USE_BRLAPI
#error This test requires brlapi.
#endif

#include <deque>

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::ProfileHelper;
using chromeos::ScreenLocker;
using chromeos::UserManager;
using chromeos::test::ScreenLockerTester;
using content::BrowserThread;

namespace extensions {
namespace api {
namespace braille_display_private {

namespace {

const char kTestUserName[] = "owner@invalid.domain";

}  // namespace

// Data maintained by the mock BrlapiConnection.  This data lives throughout
// a test, while the api implementation takes ownership of the connection
// itself.
struct MockBrlapiConnectionData {
  bool connected;
  size_t display_size;
  brlapi_error_t error;
  std::vector<std::string> written_content;
  // List of brlapi key codes.  A negative number makes the connection mock
  // return an error from ReadKey.
  std::deque<brlapi_keyCode_t> pending_keys;
  // Causes a new display to appear to appear on disconnect, that is the
  // display size doubles and the controller gets notified of a brltty
  // restart.
  bool reappear_on_disconnect;
};

class MockBrlapiConnection : public BrlapiConnection {
 public:
  explicit MockBrlapiConnection(MockBrlapiConnectionData* data)
      : data_(data) {}
  virtual ConnectResult Connect(const OnDataReadyCallback& on_data_ready)
      OVERRIDE {
    data_->connected = true;
    on_data_ready_ = on_data_ready;
    if (!data_->pending_keys.empty()) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MockBrlapiConnection::NotifyDataReady,
                                        base::Unretained(this)));
    }
    return CONNECT_SUCCESS;
  }

  virtual void Disconnect() OVERRIDE {
    data_->connected = false;
    if (data_->reappear_on_disconnect) {
      data_->display_size *= 2;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BrailleControllerImpl::PokeSocketDirForTesting,
                     base::Unretained(BrailleControllerImpl::GetInstance())));
    }
  }

  virtual bool Connected() OVERRIDE {
    return data_->connected;
  }

  virtual brlapi_error_t* BrlapiError() OVERRIDE {
    return &data_->error;
  }

  virtual std::string BrlapiStrError() OVERRIDE {
    return data_->error.brlerrno != BRLAPI_ERROR_SUCCESS ? "Error" : "Success";
  }

  virtual bool GetDisplaySize(size_t* size) OVERRIDE {
    *size = data_->display_size;
    return true;
  }

  virtual bool WriteDots(const unsigned char* cells) OVERRIDE {
    std::string written(reinterpret_cast<const char*>(cells),
                        data_->display_size);
    data_->written_content.push_back(written);
    return true;
  }

  virtual int ReadKey(brlapi_keyCode_t* key_code) OVERRIDE {
    if (!data_->pending_keys.empty()) {
      int queued_key_code = data_->pending_keys.front();
      data_->pending_keys.pop_front();
      if (queued_key_code < 0) {
        data_->error.brlerrno = BRLAPI_ERROR_EOF;
        return -1;  // Signal error.
      }
      *key_code = queued_key_code;
      return 1;
    } else {
      return 0;
    }
  }

 private:

  void NotifyDataReady() {
    on_data_ready_.Run();
    if (!data_->pending_keys.empty()) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MockBrlapiConnection::NotifyDataReady,
                                        base::Unretained(this)));
    }
  }

  MockBrlapiConnectionData* data_;
  OnDataReadyCallback on_data_ready_;
};

class BrailleDisplayPrivateApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    connection_data_.connected = false;
    connection_data_.display_size = 0;
    connection_data_.error.brlerrno = BRLAPI_ERROR_SUCCESS;
    connection_data_.reappear_on_disconnect = false;
    BrailleControllerImpl::GetInstance()->SetCreateBrlapiConnectionForTesting(
        base::Bind(
            &BrailleDisplayPrivateApiTest::CreateBrlapiConnection,
            base::Unretained(this)));
    DisableAccessibilityManagerBraille();
  }

 protected:
  MockBrlapiConnectionData connection_data_;

  // By default, don't let the accessibility manager interfere and
  // steal events.  Some tests override this to keep the normal behaviour
  // of the accessibility manager.
  virtual void DisableAccessibilityManagerBraille() {
    chromeos::AccessibilityManager::SetBrailleControllerForTest(
        &stub_braille_controller_);
  }

 private:
  scoped_ptr<BrlapiConnection> CreateBrlapiConnection() {
    return scoped_ptr<BrlapiConnection>(
        new MockBrlapiConnection(&connection_data_));
  }

  StubBrailleController stub_braille_controller_;
};

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, WriteDots) {
  connection_data_.display_size = 11;
  ASSERT_TRUE(RunComponentExtensionTest("braille_display_private/write_dots"))
      << message_;
  ASSERT_EQ(3U, connection_data_.written_content.size());
  const std::string expected_content(connection_data_.display_size, '\0');
  for (size_t i = 0; i < connection_data_.written_content.size(); ++i) {
    ASSERT_EQ(std::string(
        connection_data_.display_size,
        static_cast<char>(i)),
                 connection_data_.written_content[i])
        << "String " << i << " doesn't match";
  }
}

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, KeyEvents) {
  connection_data_.display_size = 11;
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_LNUP);
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_LNDN);
  ASSERT_TRUE(RunComponentExtensionTest("braille_display_private/key_events"));
}

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, DisplayStateChanges) {
  connection_data_.display_size = 11;
  connection_data_.pending_keys.push_back(-1);
  connection_data_.reappear_on_disconnect = true;
  ASSERT_TRUE(RunComponentExtensionTest(
      "braille_display_private/display_state_changes"));
}

class BrailleDisplayPrivateAPIUserTest : public BrailleDisplayPrivateApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  class MockEventDelegate : public BrailleDisplayPrivateAPI::EventDelegate {
   public:
    MockEventDelegate() : event_count_(0) {}

    int GetEventCount() { return event_count_; }

    virtual void BroadcastEvent(scoped_ptr<Event> event) OVERRIDE {
      ++event_count_;
    }
    virtual bool HasListener() OVERRIDE { return true; }

   private:
    int event_count_;
  };

  MockEventDelegate* SetMockEventDelegate(BrailleDisplayPrivateAPI* api) {
    MockEventDelegate* delegate = new MockEventDelegate();
    api->SetEventDelegateForTest(
        scoped_ptr<BrailleDisplayPrivateAPI::EventDelegate>(delegate).Pass());
    return delegate;
  }

  void LockScreen(ScreenLockerTester* tester) {
    ScreenLocker::Show();
    tester->EmulateWindowManagerReady();
    content::WindowedNotificationObserver lock_state_observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    if (!tester->IsLocked())
      lock_state_observer.Wait();
    ASSERT_TRUE(tester->IsLocked());
  }

  void DismissLockScreen(ScreenLockerTester* tester) {
    ScreenLocker::Hide();
    content::WindowedNotificationObserver lock_state_observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    if (tester->IsLocked())
      lock_state_observer.Wait();
    ASSERT_FALSE(tester->IsLocked());
  }

 protected:
  virtual void DisableAccessibilityManagerBraille() OVERRIDE {
    // Let the accessibility manager behave as usual for these tests.
  }
};

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateAPIUserTest,
                       KeyEventOnLockScreen) {
  scoped_ptr<ScreenLockerTester> tester(ScreenLocker::GetTester());
  // Log in.
  UserManager::Get()->UserLoggedIn(kTestUserName, kTestUserName, true);
  UserManager::Get()->SessionStarted();
  ASSERT_FALSE(
      ProfileHelper::GetSigninProfile()->IsSameProfile(
          ProfileManager::GetDefaultProfile()))
      << ProfileHelper::GetSigninProfile()->GetDebugName() << " vs. "
      << ProfileManager::GetDefaultProfile()->GetDebugName();

  // Create API and event delegate for sign in profile.
  BrailleDisplayPrivateAPI signin_api(ProfileHelper::GetSigninProfile());
  MockEventDelegate* signin_delegate = SetMockEventDelegate(&signin_api);
  EXPECT_EQ(0, signin_delegate->GetEventCount());
  // Create api and delegate for the logged in user.
  BrailleDisplayPrivateAPI user_api(ProfileManager::GetDefaultProfile());
  MockEventDelegate* user_delegate = SetMockEventDelegate(&user_api);

  // Send key event to both profiles.
  KeyEvent key_event;
  key_event.command = KEY_COMMAND_LINE_UP;
  signin_api.OnKeyEvent(key_event);
  user_api.OnKeyEvent(key_event);
  EXPECT_EQ(0, signin_delegate->GetEventCount());
  EXPECT_EQ(1, user_delegate->GetEventCount());

  // Lock screen, and make sure that the key event goes to the
  // signin profile.
  LockScreen(tester.get());
  signin_api.OnKeyEvent(key_event);
  user_api.OnKeyEvent(key_event);
  EXPECT_EQ(1, signin_delegate->GetEventCount());
  EXPECT_EQ(1, user_delegate->GetEventCount());

  // Unlock screen, making sur ekey events go to the user profile again.
  DismissLockScreen(tester.get());
  signin_api.OnKeyEvent(key_event);
  user_api.OnKeyEvent(key_event);
  EXPECT_EQ(1, signin_delegate->GetEventCount());
  EXPECT_EQ(2, user_delegate->GetEventCount());
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
