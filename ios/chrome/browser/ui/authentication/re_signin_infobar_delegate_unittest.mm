// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ReSignInInfoBarDelegateTest : public PlatformTest {
 public:
  ReSignInInfoBarDelegateTest() {}

 protected:
  void SetUp() override {
    mainBrowserStateBuilder_.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFake::CreateAuthenticationService);
  }

  void SetUpMainChromeBrowserStateNotSignedIn() {
    chrome_browser_state_ = mainBrowserStateBuilder_.Build();
  }

  void SetUpMainChromeBrowserStateWithSignedInUser() {
    ChromeIdentity* chrome_identity =
        [FakeChromeIdentity identityWithEmail:@"john.appleseed@gmail.com"
                                       gaiaID:@"1234"
                                         name:@"John"];

    chrome_browser_state_ = mainBrowserStateBuilder_.Build();
    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    authService->SignIn(chrome_identity, std::string());
  }

  void SetUpMainChromeBrowserStateWithIncognito() {
    SetUpMainChromeBrowserStateNotSignedIn();
    // Sets up an incognito ChromeBrowserState and attaches it to the main one.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::TestWebThreadBundle thread_bundle_;

  TestChromeBrowserState::Builder mainBrowserStateBuilder_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenNotPrompting) {
  // User is not signed in, but the "prompt" flag is not set.
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authService->SetPromptForSignIn(false);
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), nil);
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate.get());
  EXPECT_FALSE(authService->ShouldPromptForSignIn());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenNotSignedIn) {
  // User is not signed in, but the "prompt" flag is set.
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authService->SetPromptForSignIn(true);
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), nil);
  // Infobar delegate should be created.
  EXPECT_TRUE(infobar_delegate.get());
  EXPECT_TRUE(authService->ShouldPromptForSignIn());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenAlreadySignedIn) {
  // User is signed in and the "prompt" flag is set.
  SetUpMainChromeBrowserStateWithSignedInUser();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authService->SetPromptForSignIn(true);
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), nil);
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate.get());
  EXPECT_FALSE(authService->ShouldPromptForSignIn());
}

TEST_F(ReSignInInfoBarDelegateTest, TestCreateWhenIncognito) {
  // Tab is incognito, and the "prompt" flag is set.
  SetUpMainChromeBrowserStateWithIncognito();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authService->SetPromptForSignIn(true);
  std::unique_ptr<ReSignInInfoBarDelegate> infobar_delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_->GetOffTheRecordChromeBrowserState(), nil);
  // Infobar delegate should not be created.
  EXPECT_FALSE(infobar_delegate.get());
  EXPECT_TRUE(authService->ShouldPromptForSignIn());
}

TEST_F(ReSignInInfoBarDelegateTest, TestMessages) {
  SetUpMainChromeBrowserStateNotSignedIn();
  std::unique_ptr<ReSignInInfoBarDelegate> delegate(
      new ReSignInInfoBarDelegate(chrome_browser_state_.get(), nil));
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, delegate->GetButtons());
  base::string16 message_text = delegate->GetMessageText();
  EXPECT_GT(message_text.length(), 0U);
  base::string16 button_label =
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_GT(button_label.length(), 0U);
}

TEST_F(ReSignInInfoBarDelegateTest, TestAccept) {
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authService->SetPromptForSignIn(true);

  id dispatcher = OCMProtocolMock(@protocol(ApplicationCommands));
  [[dispatcher expect]
      showSignin:[OCMArg checkWithBlock:^BOOL(id command) {
        EXPECT_TRUE([command isKindOfClass:[ShowSigninCommand class]]);
        EXPECT_EQ(AUTHENTICATION_OPERATION_REAUTHENTICATE,
                  static_cast<ShowSigninCommand*>(command).operation);
        return YES;
      }]];

  std::unique_ptr<infobars::InfoBar> infobar(
      CreateConfirmInfoBar(ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), dispatcher)));
  InfoBarIOS* infobarIOS = static_cast<InfoBarIOS*>(infobar.get());
  infobarIOS->Layout(CGRectZero);

  ReSignInInfoBarDelegate* delegate =
      static_cast<ReSignInInfoBarDelegate*>(infobarIOS->delegate());
  EXPECT_TRUE(delegate->Accept());
  EXPECT_FALSE(authService->ShouldPromptForSignIn());
}

TEST_F(ReSignInInfoBarDelegateTest, TestInfoBarDismissed) {
  SetUpMainChromeBrowserStateNotSignedIn();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authService->SetPromptForSignIn(true);

  id dispatcher = OCMProtocolMock(@protocol(ApplicationCommands));
  [[dispatcher reject] showSignin:[OCMArg any]];

  std::unique_ptr<infobars::InfoBar> infobar(
      CreateConfirmInfoBar(ReSignInInfoBarDelegate::CreateInfoBarDelegate(
          chrome_browser_state_.get(), dispatcher)));
  InfoBarIOS* infobarIOS = static_cast<InfoBarIOS*>(infobar.get());
  infobarIOS->Layout(CGRectZero);

  ReSignInInfoBarDelegate* delegate =
      static_cast<ReSignInInfoBarDelegate*>(infobarIOS->delegate());
  delegate->InfoBarDismissed();
  EXPECT_FALSE(authService->ShouldPromptForSignIn());
}

}  // namespace
