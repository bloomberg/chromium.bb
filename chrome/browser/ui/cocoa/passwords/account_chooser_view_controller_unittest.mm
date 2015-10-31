// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/account_chooser_more_combobox_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

@interface ManagePasswordsBubbleAccountChooserViewController(Testing)
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) BubbleCombobox* moreButton;
@property(nonatomic, readonly) NSTableView* credentialsView;
@end

@interface CredentialItemView(Testing)
@property(nonatomic, readonly) NSTextField* upperLabel;
@end

@interface AccountAvatarFetcherTestManager : AccountAvatarFetcherManager {
  std::vector<GURL> fetchedAvatars_;
}
@property(nonatomic, readonly) const std::vector<GURL>& fetchedAvatars;
@end

@implementation AccountAvatarFetcherTestManager

- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view {
  fetchedAvatars_.push_back(avatarURL);
}

- (const std::vector<GURL>&)fetchedAvatars {
  return fetchedAvatars_;
}

@end

namespace {

// Returns a PasswordForm with only a username.
scoped_ptr<autofill::PasswordForm> Credential(const char* username) {
  scoped_ptr<autofill::PasswordForm> credential(new autofill::PasswordForm);
  credential->username_value = base::ASCIIToUTF16(username);
  return credential.Pass();
}

// Tests for the account chooser view of the password management bubble.
class ManagePasswordsBubbleAccountChooserViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleAccountChooserViewControllerTest() : controller_(nil) {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
    avatar_manager_.reset([[AccountAvatarFetcherTestManager alloc] init]);
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  AccountAvatarFetcherTestManager* avatar_manager() {
    return avatar_manager_.get();
  }

  ManagePasswordsBubbleAccountChooserViewController* controller() {
    if (!controller_) {
      controller_.reset(
          [[ManagePasswordsBubbleAccountChooserViewController alloc]
              initWithModel:model()
              avatarManager:avatar_manager()
                   delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<AccountAvatarFetcherTestManager> avatar_manager_;
  base::scoped_nsobject<ManagePasswordsBubbleAccountChooserViewController>
      controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
};

TEST_F(ManagePasswordsBubbleAccountChooserViewControllerTest, ConfiguresViews) {
  ScopedVector<autofill::PasswordForm> local_forms;
  local_forms.push_back(Credential("pizza"));
  ScopedVector<autofill::PasswordForm> federated_forms;
  federated_forms.push_back(Credential("taco"));
  EXPECT_TRUE(ui_controller()->OnChooseCredentials(
      local_forms.Pass(),
      federated_forms.Pass(),
      GURL("http://example.com"),
      base::Callback<void(const password_manager::CredentialInfo&)>()));
  // Trigger creation of controller and check the views.
  NSTableView* view = controller().credentialsView;
  ASSERT_NSNE(nil, view);
  ASSERT_EQ(2U, view.numberOfRows);
  EXPECT_NSEQ(
      @"pizza",
      base::mac::ObjCCastStrict<CredentialItemView>(
          base::mac::ObjCCastStrict<CredentialItemCell>(
              [view.delegate tableView:view dataCellForTableColumn:nil row:0])
              .view).upperLabel.stringValue);
  EXPECT_NSEQ(
      @"taco",
      base::mac::ObjCCastStrict<CredentialItemView>(
          base::mac::ObjCCastStrict<CredentialItemCell>(
              [view.delegate tableView:view dataCellForTableColumn:nil row:1])
              .view).upperLabel.stringValue);
  EXPECT_TRUE(avatar_manager().fetchedAvatars.empty());
}

TEST_F(ManagePasswordsBubbleAccountChooserViewControllerTest,
       ForwardsAvatarFetchToManager) {
  ScopedVector<autofill::PasswordForm> local_forms;
  local_forms.push_back(Credential("taco"));
  local_forms.back()->icon_url = GURL("http://foo");
  EXPECT_TRUE(ui_controller()->OnChooseCredentials(
      local_forms.Pass(),
      ScopedVector<autofill::PasswordForm>(),
      GURL("http://example.com"),
      base::Callback<void(const password_manager::CredentialInfo&)>()));
  // Trigger creation of the controller and check the fetched URLs.
  controller();
  EXPECT_FALSE(avatar_manager().fetchedAvatars.empty());
  EXPECT_TRUE(std::find(avatar_manager().fetchedAvatars.begin(),
                        avatar_manager().fetchedAvatars.end(),
                        GURL("http://foo")) !=
              avatar_manager().fetchedAvatars.end());
}

TEST_F(ManagePasswordsBubbleAccountChooserViewControllerTest,
       SelectingCredentialInformsModelAndClosesDialog) {
  ScopedVector<autofill::PasswordForm> local_forms;
  local_forms.push_back(Credential("pizza"));
  ScopedVector<autofill::PasswordForm> federated_forms;
  federated_forms.push_back(Credential("taco"));
  EXPECT_TRUE(ui_controller()->OnChooseCredentials(
      local_forms.Pass(),
      federated_forms.Pass(),
      GURL("http://example.com"),
      base::Callback<void(const password_manager::CredentialInfo&)>()));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            ui_controller()->state());
  [controller().credentialsView
          selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
      byExtendingSelection:NO];
  EXPECT_TRUE(delegate().dismissed);
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            ui_controller()->state());
  EXPECT_TRUE(ui_controller()->choose_credential());
  EXPECT_EQ(base::ASCIIToUTF16("taco"),
            ui_controller()->chosen_credential().username_value);
}

TEST_F(ManagePasswordsBubbleAccountChooserViewControllerTest,
       SelectingNopeDismissesDialog) {
  ScopedVector<autofill::PasswordForm> local_forms;
  local_forms.push_back(Credential("pizza"));
  EXPECT_TRUE(ui_controller()->OnChooseCredentials(
      local_forms.Pass(), ScopedVector<autofill::PasswordForm>(),
      GURL("http://example.com"),
      base::Callback<void(const password_manager::CredentialInfo&)>()));
  [controller().cancelButton performClick:nil];
  EXPECT_TRUE(delegate().dismissed);
}

TEST_F(ManagePasswordsBubbleAccountChooserViewControllerTest,
       SelectingSettingsShowsSettingsPage) {
  BubbleCombobox* moreButton = controller().moreButton;
  EXPECT_TRUE(moreButton);
  [[moreButton menu] performActionForItemAtIndex:
                         AccountChooserMoreComboboxModel::INDEX_SETTINGS];
  EXPECT_TRUE(ui_controller()->navigated_to_settings_page());
  EXPECT_TRUE(delegate().dismissed);
}

TEST_F(ManagePasswordsBubbleAccountChooserViewControllerTest,
       SelectingLearnMoreShowsHelpCenterArticle) {
  BubbleCombobox* moreButton = controller().moreButton;
  EXPECT_TRUE(moreButton);
  [[moreButton menu] performActionForItemAtIndex:
                         AccountChooserMoreComboboxModel::INDEX_LEARN_MORE];
  EXPECT_TRUE(delegate().dismissed);
  // TODO(dconnelly): Test this when the article is written.
}

}  // namespace
