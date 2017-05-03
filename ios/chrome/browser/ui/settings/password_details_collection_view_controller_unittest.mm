// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/password_details_item.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MockReauthenticationModule : NSObject<ReauthenticationProtocol>

@property(nonatomic, copy) NSString* localizedReasonForAuthentication;

@end

@implementation MockReauthenticationModule

@synthesize localizedReasonForAuthentication =
    _localizedReasonForAuthentication;

- (BOOL)canAttemptReauth {
  return YES;
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                                 handler:(void (^)(BOOL success))
                                             showCopyPasswordsHandler {
  self.localizedReasonForAuthentication = localizedReason;
  showCopyPasswordsHandler(YES);
}

@end

@interface MockSavePasswordsCollectionViewController
    : NSObject<PasswordDetailsCollectionViewControllerDelegate>

- (void)deletePassword:(const autofill::PasswordForm&)passwordForm;

@property(nonatomic) NSInteger numberOfCallsToDeletePassword;

@end

@implementation MockSavePasswordsCollectionViewController

@synthesize numberOfCallsToDeletePassword = _numberOfCallsToDeletePassword;

- (void)deletePassword:(const autofill::PasswordForm&)passwordForm {
  ++_numberOfCallsToDeletePassword;
}

@end

namespace {

NSString* const kSite = @"https://testorigin.com/";
NSString* const kUsername = @"testusername";
NSString* const kPassword = @"testpassword";

const int kSiteSection = 0;
const int kSiteItem = 0;
const int kCopySiteButtonItem = 1;

const int kUsernameSection = 1;
const int kUsernameItem = 0;
const int kCopyUsernameButtonItem = 1;

const int kPasswordSection = 2;
const int kPasswordItem = 0;
const int kShowHideButtonItem = 1;
const int kCopyPasswordButtonItem = 2;

const int kDeleteSection = 3;
const int kDeleteButtonItem = 0;

class PasswordDetailsCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  PasswordDetailsCollectionViewControllerTest()
      : thread_bundle_(web::TestWebThreadBundle::REAL_DB_THREAD) {}
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    origin_ = kSite;
    delegate_ = [[MockSavePasswordsCollectionViewController alloc] init];
    reauthenticationModule_ = [[MockReauthenticationModule alloc] init];
  }

  CollectionViewController* InstantiateController() override {
    autofill::PasswordForm form;
    form.username_value = base::SysNSStringToUTF16(kUsername);
    form.password_value = base::SysNSStringToUTF16(kPassword);
    form.signon_realm = base::SysNSStringToUTF8(origin_);
    form.origin = GURL(form.signon_realm);
    return [[PasswordDetailsCollectionViewController alloc]
          initWithPasswordForm:form
                      delegate:delegate_
        reauthenticationModule:reauthenticationModule_
                      username:kUsername
                      password:kPassword
                        origin:origin_];
  }

  void CreateControllerWithOrigin(NSString* test_origin) {
    origin_ = test_origin;
    CreateController();
  }

  web::TestWebThreadBundle thread_bundle_;
  MockSavePasswordsCollectionViewController* delegate_;
  MockReauthenticationModule* reauthenticationModule_;
  NSString* origin_;
};

TEST_F(PasswordDetailsCollectionViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();
  EXPECT_EQ(4, NumberOfSections());
  // Site section
  EXPECT_EQ(2, NumberOfItemsInSection(kUsernameSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_SITE, kSiteSection);
  PasswordDetailsItem* siteItem =
      GetCollectionViewItem(kSiteSection, kSiteItem);
  EXPECT_NSEQ(origin_, siteItem.text);
  EXPECT_TRUE(siteItem.showingText);
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_SITE_COPY_BUTTON, kSiteSection,
                           kCopySiteButtonItem);
  // Username section
  EXPECT_EQ(2, NumberOfItemsInSection(kUsernameSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME,
                           kUsernameSection);
  PasswordDetailsItem* usernameItem =
      GetCollectionViewItem(kUsernameSection, kUsernameItem);
  EXPECT_NSEQ(kUsername, usernameItem.text);
  EXPECT_TRUE(usernameItem.showingText);
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON,
                           kUsernameSection, kCopyUsernameButtonItem);
  // Password section
  EXPECT_EQ(3, NumberOfItemsInSection(kPasswordSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD,
                           kPasswordSection);
  PasswordDetailsItem* passwordItem =
      GetCollectionViewItem(kPasswordSection, kPasswordItem);
  EXPECT_NSEQ(kPassword, passwordItem.text);
  EXPECT_FALSE(passwordItem.showingText);
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON,
                           kPasswordSection, kShowHideButtonItem);
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON,
                           kPasswordSection, kCopyPasswordButtonItem);
  // Delete section
  EXPECT_EQ(1, NumberOfItemsInSection(kDeleteSection));
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON,
                           kDeleteSection, kDeleteButtonItem);
}

struct SimplifyOriginTestData {
  NSString* origin;
  NSString* expectedSimplifiedOrigin;
};

TEST_F(PasswordDetailsCollectionViewControllerTest, SimplifyOrigin) {
  SimplifyOriginTestData test_data[] = {
      {@"http://test.com/index.php", @"test.com"},
      {@"test.com/index.php", @"test.com"},
      {@"test.com", @"test.com"}};

  for (size_t i = 0; i < arraysize(test_data); i++) {
    SimplifyOriginTestData& data = test_data[i];
    CreateControllerWithOrigin(data.origin);
    EXPECT_NSEQ(data.expectedSimplifiedOrigin, controller().title)
        << " for origin " << base::SysNSStringToUTF8(test_data[i].origin);
    ResetController();
  }
}

TEST_F(PasswordDetailsCollectionViewControllerTest, CopySite) {
  CreateController();
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:kCopySiteButtonItem
                                                  inSection:kSiteSection]];
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(origin_, generalPasteboard.string);
}

TEST_F(PasswordDetailsCollectionViewControllerTest, CopyUsername) {
  CreateController();
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath
                                   indexPathForRow:kCopyUsernameButtonItem
                                         inSection:kUsernameSection]];
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(kUsername, generalPasteboard.string);
}

TEST_F(PasswordDetailsCollectionViewControllerTest, ShowPassword) {
  CreateController();
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:kShowHideButtonItem
                                                  inSection:kPasswordSection]];
  PasswordDetailsItem* passwordItem =
      GetCollectionViewItem(kPasswordSection, kPasswordItem);
  EXPECT_NSEQ(kPassword, passwordItem.text);
  EXPECT_TRUE(passwordItem.showingText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW),
      reauthenticationModule_.localizedReasonForAuthentication);
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON,
                           kPasswordSection, kShowHideButtonItem);
}

TEST_F(PasswordDetailsCollectionViewControllerTest, HidePassword) {
  CreateController();
  // First show the password.
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:kShowHideButtonItem
                                                  inSection:kPasswordSection]];
  // Then hide it.
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:kShowHideButtonItem
                                                  inSection:kPasswordSection]];
  PasswordDetailsItem* passwordItem =
      GetCollectionViewItem(kPasswordSection, kPasswordItem);
  EXPECT_NSEQ(kPassword, passwordItem.text);
  EXPECT_FALSE(passwordItem.showingText);
  CheckTextCellTitleWithId(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON,
                           kPasswordSection, kShowHideButtonItem);
}

TEST_F(PasswordDetailsCollectionViewControllerTest, CopyPassword) {
  CreateController();
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath
                                   indexPathForRow:kCopyPasswordButtonItem
                                         inSection:kPasswordSection]];
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(kPassword, generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY),
      reauthenticationModule_.localizedReasonForAuthentication);
}

TEST_F(PasswordDetailsCollectionViewControllerTest, DeletePassword) {
  CreateController();
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:kDeleteButtonItem
                                                  inSection:kDeleteSection]];
  EXPECT_EQ(1, delegate_.numberOfCallsToDeletePassword);
}

}  // namespace
