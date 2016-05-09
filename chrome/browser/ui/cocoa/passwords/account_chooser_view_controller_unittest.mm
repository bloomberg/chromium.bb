// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"

#include <utility>

#include <Carbon/Carbon.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_button.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

@interface AccountAvatarFetcherTestManager : AccountAvatarFetcherManager {
  std::vector<GURL> fetchedAvatars_;
}
@property(nonatomic, readonly) const std::vector<GURL>& fetchedAvatars;
@end

@implementation AccountAvatarFetcherTestManager

- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemButton*)view {
  fetchedAvatars_.push_back(avatarURL);
}

- (const std::vector<GURL>&)fetchedAvatars {
  return fetchedAvatars_;
}

@end

namespace {

constexpr char kDialogTitle[] = "Choose an account";

// Returns a PasswordForm with only a username.
std::unique_ptr<autofill::PasswordForm> Credential(const char* username) {
  std::unique_ptr<autofill::PasswordForm> credential(
      new autofill::PasswordForm);
  credential->username_value = base::ASCIIToUTF16(username);
  return credential;
}

class PasswordDialogControllerMock : public PasswordDialogController {
 public:
  MOCK_CONST_METHOD0(GetLocalForms, const FormsVector&());
  MOCK_CONST_METHOD0(GetFederationsForms, const FormsVector&());
  MOCK_CONST_METHOD0(GetAccoutChooserTitle,
                     std::pair<base::string16, gfx::Range>());
  MOCK_CONST_METHOD0(GetAutoSigninPromoTitle, base::string16());
  MOCK_CONST_METHOD0(GetAutoSigninText,
                     std::pair<base::string16, gfx::Range>());
  MOCK_METHOD0(OnSmartLockLinkClicked, void());
  MOCK_METHOD2(OnChooseCredentials, void(
      const autofill::PasswordForm& password_form,
      password_manager::CredentialType credential_type));
  MOCK_METHOD0(OnAutoSigninOK, void());
  MOCK_METHOD0(OnAutoSigninTurnOff, void());
  MOCK_METHOD0(OnCloseDialog, void());
};

// Tests for the account chooser dialog view.
class AccountChooserViewControllerTest : public CocoaTest,
                                         public PasswordPromptBridgeInterface {
 public:
  void SetUp() override;

  PasswordDialogControllerMock& dialog_controller() {
    return dialog_controller_;
  }

  AccountAvatarFetcherTestManager* avatar_manager() {
    return avatar_manager_.get();
  }

  AccountChooserViewController* view_controller() {
    return view_controller_.get();
  }

  void SetUpAccountChooser(PasswordDialogController::FormsVector* local);

  MOCK_METHOD0(OnPerformClose, void());

  // PasswordPromptBridgeInterface:
  void PerformClose() override;
  PasswordDialogController* GetDialogController() override;
  net::URLRequestContextGetter* GetRequestContext() const override;

 private:
  PasswordDialogControllerMock dialog_controller_;
  base::scoped_nsobject<AccountAvatarFetcherTestManager> avatar_manager_;
  base::scoped_nsobject<AccountChooserViewController> view_controller_;
};

void AccountChooserViewControllerTest::SetUp() {
  CocoaTest::SetUp();
  avatar_manager_.reset([[AccountAvatarFetcherTestManager alloc] init]);
}

void AccountChooserViewControllerTest::SetUpAccountChooser(
    PasswordDialogController::FormsVector* local) {
  view_controller_.reset([[AccountChooserViewController alloc]
      initWithBridge:this
      avatarManager:avatar_manager()]);
  EXPECT_CALL(dialog_controller_, GetLocalForms())
      .WillOnce(testing::ReturnRef(*local));
  EXPECT_CALL(dialog_controller_, GetAccoutChooserTitle())
      .WillOnce(testing::Return(std::make_pair(base::ASCIIToUTF16(kDialogTitle),
                                               gfx::Range(0, 5))));
  [view_controller_ view];
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&dialog_controller_));
}

void AccountChooserViewControllerTest::PerformClose() {
  view_controller_.reset();
  OnPerformClose();
}

PasswordDialogController*
AccountChooserViewControllerTest::GetDialogController() {
  return &dialog_controller_;
}

net::URLRequestContextGetter*
AccountChooserViewControllerTest::GetRequestContext() const {
  NOTREACHED();
  return nullptr;
}

TEST_F(AccountChooserViewControllerTest, ConfiguresViews) {
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  local_forms.push_back(Credential("taco"));
  SetUpAccountChooser(&local_forms);

  NSArray* buttons = view_controller().credentialButtons;
  ASSERT_NSNE(nil, buttons);
  ASSERT_EQ(2u, buttons.count);
  EXPECT_NSEQ(@"pizza", [base::mac::ObjCCastStrict<CredentialItemButton>(
                            [buttons objectAtIndex:0]) title]);
  EXPECT_NSEQ(@"taco", [base::mac::ObjCCastStrict<CredentialItemButton>(
                           [buttons objectAtIndex:1]) title]);
  EXPECT_TRUE(avatar_manager().fetchedAvatars.empty());
}

TEST_F(AccountChooserViewControllerTest, ConfiguresFederatedCredential) {
  const char federation[] = "https://google.com/idp";
  const char name[] = "Peter the Great";
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  local_forms.back()->federation_origin = url::Origin(GURL(federation));
  local_forms.back()->display_name = base::ASCIIToUTF16(name);
  SetUpAccountChooser(&local_forms);

  NSArray* buttons = view_controller().credentialButtons;
  ASSERT_NSNE(nil, buttons);
  ASSERT_EQ(1u, buttons.count);
  std::string title =
      base::SysNSStringToUTF8([base::mac::ObjCCastStrict<CredentialItemButton>(
          [buttons objectAtIndex:0]) title]);
  EXPECT_THAT(title, testing::HasSubstr(name));
  EXPECT_THAT(title, testing::HasSubstr(GURL(federation).host()));
}

TEST_F(AccountChooserViewControllerTest, ForwardsAvatarFetchToManager) {
  PasswordDialogController::FormsVector local_forms;
  std::unique_ptr<autofill::PasswordForm> form = Credential("taco");
  form->icon_url = GURL("http://foo.com");
  local_forms.push_back(std::move(form));
  SetUpAccountChooser(&local_forms);
  EXPECT_FALSE(avatar_manager().fetchedAvatars.empty());
  EXPECT_TRUE(std::find(avatar_manager().fetchedAvatars.begin(),
                        avatar_manager().fetchedAvatars.end(),
                        GURL("http://foo.com")) !=
              avatar_manager().fetchedAvatars.end());
}

TEST_F(AccountChooserViewControllerTest,
       SelectingCredentialInformsModelAndClosesDialog) {
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  local_forms.push_back(Credential("taco"));
  SetUpAccountChooser(&local_forms);
  EXPECT_CALL(dialog_controller(),
              OnChooseCredentials(
                  *Credential("taco"),
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  [base::mac::ObjCCastStrict<CredentialItemButton>(
      [view_controller().credentialButtons objectAtIndex:1]) performClick:nil];
}

TEST_F(AccountChooserViewControllerTest, SelectingNopeDismissesDialog) {
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  SetUpAccountChooser(&local_forms);
  EXPECT_CALL(*this, OnPerformClose());
  [view_controller().cancelButton performClick:nil];
}

TEST_F(AccountChooserViewControllerTest, ClickTitleLink) {
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  SetUpAccountChooser(&local_forms);
  EXPECT_CALL(dialog_controller(), OnSmartLockLinkClicked());
  [view_controller().titleView clickedOnLink:@""
                                     atIndex:0];
}

TEST_F(AccountChooserViewControllerTest, ClosePromptAndHandleClick) {
  // A user may press mouse down, some navigation closes the dialog, mouse up
  // still sends the action. The view should not crash.
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  SetUpAccountChooser(&local_forms);
  [view_controller() setBridge:nil];
  [view_controller().titleView clickedOnLink:@"" atIndex:0];
  [base::mac::ObjCCastStrict<CredentialItemButton>(
      [view_controller().credentialButtons objectAtIndex:0]) performClick:nil];
  [view_controller().cancelButton performClick:nil];
}

TEST_F(AccountChooserViewControllerTest, CloseOnEsc) {
  PasswordDialogController::FormsVector local_forms;
  local_forms.push_back(Credential("pizza"));
  SetUpAccountChooser(&local_forms);
  EXPECT_CALL(*this, OnPerformClose());
  [[view_controller() view]
      performKeyEquivalent:cocoa_test_event_utils::KeyEventWithKeyCode(
                               kVK_Escape, '\e', NSKeyDown, 0)];
}

}  // namespace
