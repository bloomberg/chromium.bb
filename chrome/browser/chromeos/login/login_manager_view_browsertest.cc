// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

const char kUsername[] = "test_user@gmail.com";
const char kPassword[] = "test_password";

class LoginManagerViewTest : public WizardInProcessBrowserTest {
 public:
  LoginManagerViewTest(): WizardInProcessBrowserTest("login") {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    InitStatusAreaMocks();
    SetStatusAreaMocksExpectations();

    mock_login_library_ = new MockLoginLibrary();
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);
    test_api()->SetLoginLibrary(mock_login_library_, true);

    mock_cryptohome_library_ = new MockCryptohomeLibrary();
    EXPECT_CALL(*mock_cryptohome_library_, IsMounted())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    test_api()->SetCryptohomeLibrary(mock_cryptohome_library_, true);

    LoginUtils::Set(new MockLoginUtils(kUsername, kPassword));
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    test_api()->SetLoginLibrary(NULL, false);
    test_api()->SetCryptohomeLibrary(NULL, false);
  }

 private:
  MockLoginLibrary* mock_login_library_;
  MockCryptohomeLibrary* mock_cryptohome_library_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerViewTest);
};

static void Quit() {
  LOG(INFO) << "Posting a QuitTask to UI thread";
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE, new MessageLoop::QuitTask);
}
IN_PROC_BROWSER_TEST_F(LoginManagerViewTest, TestBasic) {
  ASSERT_TRUE(controller() != NULL);
  ASSERT_EQ(controller()->current_screen(), controller()->GetLoginScreen());

  scoped_ptr<MockScreenObserver> mock_screen_observer(
      new MockScreenObserver());
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::LOGIN_SIGN_IN_SELECTED))
      .WillOnce(InvokeWithoutArgs(Quit));

  LoginManagerView* login = controller()->GetLoginScreen()->view();
  login->set_observer(mock_screen_observer.get());
  login->SetUsername(kUsername);
  login->SetPassword(kPassword);

  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  login->Login();
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  login->set_observer(NULL);
}

IN_PROC_BROWSER_TEST_F(LoginManagerViewTest, AuthenticationFailed) {
  ASSERT_TRUE(controller() != NULL);
  ASSERT_EQ(controller()->current_screen(), controller()->GetLoginScreen());

  scoped_ptr<MockScreenObserver> mock_screen_observer(
      new MockScreenObserver());

  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));

  LoginManagerView* login = controller()->GetLoginScreen()->view();
  login->set_observer(mock_screen_observer.get());
  login->SetUsername(kUsername);
  login->SetPassword("wrong password");

  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  login->Login();
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  ASSERT_EQ(controller()->current_screen(), controller()->GetLoginScreen());
  ASSERT_EQ(login->error_id(), IDS_LOGIN_ERROR_AUTHENTICATING);
  login->set_observer(NULL);
}

}  // namespace chromeos
