// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/gfx/image/image.h"

@interface CredentialItemView(Testing)
@property(nonatomic, readonly) NSTextField* nameLabel;
@property(nonatomic, readonly) NSTextField* usernameLabel;
@property(nonatomic, readonly) NSImageView* avatarView;
@end

// A test implementation of a CredentialItemDelegate to stub out interactions.
@interface CredentialItemTestDelegate : NSObject<CredentialItemDelegate>
@property(nonatomic, readonly) BOOL didFetchAvatar;
@property(nonatomic, readonly) GURL fetchedAvatarURL;
@property(nonatomic, readonly) CredentialItemView* viewForFetchedAvatar;
@property(nonatomic, readonly) BOOL didSelectPasswordForm;
@property(nonatomic, readonly) autofill::PasswordForm selectedPasswordForm;
@property(nonatomic, readonly)
    password_manager::CredentialType selectedCredentialType;
@end

@implementation CredentialItemTestDelegate
@synthesize didFetchAvatar = didFetchAvatar_;
@synthesize fetchedAvatarURL = fetchedAvatarURL_;
@synthesize viewForFetchedAvatar = viewForFetchedAvatar_;
@synthesize didSelectPasswordForm = didSelectPasswordForm_;
@synthesize selectedPasswordForm = selectedPasswordForm_;
@synthesize selectedCredentialType = selectedCredentialType_;

- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view {
  didFetchAvatar_ = YES;
  fetchedAvatarURL_ = avatarURL;
  viewForFetchedAvatar_ = view;
}

- (void)selectPasswordForm:(const autofill::PasswordForm&)passwordForm
            credentialType:(password_manager::CredentialType)credentialType {
  didSelectPasswordForm_ = YES;
  selectedPasswordForm_ = passwordForm;
  selectedCredentialType_ = credentialType;
}

@end

namespace {

// Determines whether |left| and |right| have the same data representation.
// Necessary because [CredentialItemView defaultAvatar] does some ImageSkia
// stuff that creates new NSImage instances.
bool ImagesEqual(NSImage* left, NSImage* right) {
  if (!left || !right)
    return left == right;

  gfx::Image leftImage([left copy]);
  gfx::Image rightImage([right copy]);
  return leftImage.As1xPNGBytes()->Equals(rightImage.As1xPNGBytes());
}

// Returns a PasswordForm with only a username.
autofill::PasswordForm BasicCredential() {
  autofill::PasswordForm credential;
  credential.username_value = base::ASCIIToUTF16("taco");
  return credential;
}

// Returns a PasswordForm with a username and display name.
autofill::PasswordForm CredentialWithName() {
  autofill::PasswordForm credential;
  credential.username_value = base::ASCIIToUTF16("pizza");
  credential.display_name = base::ASCIIToUTF16("margherita pizza");
  return credential;
}

// Returns a PasswordForm with a username and avatar URL.
autofill::PasswordForm CredentialWithAvatar() {
  autofill::PasswordForm credential;
  credential.username_value = base::ASCIIToUTF16("sandwich");
  credential.avatar_url = GURL("http://sandwich.com/pastrami.jpg");
  return credential;
}

// Returns a PasswordForm with a username, display name, and avatar URL.
autofill::PasswordForm CredentialWithNameAndAvatar() {
  autofill::PasswordForm credential;
  credential.username_value = base::ASCIIToUTF16("noodle");
  credential.display_name = base::ASCIIToUTF16("pasta amatriciana");
  credential.avatar_url = GURL("http://pasta.com/amatriciana.png");
  return credential;
}

// Tests for CredentialItemViewTest.
class CredentialItemViewTest : public CocoaTest {
 protected:
  void SetUp() override {
    delegate_.reset([[CredentialItemTestDelegate alloc] init]);
  }

  // Returns a delegate for testing.
  CredentialItemTestDelegate* delegate() { return delegate_.get(); }

  // Returns an autoreleased view populated from |form|.
  CredentialItemView* view(const autofill::PasswordForm& form) {
    return [[[CredentialItemView alloc]
        initWithPasswordForm:form
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_LOCAL
                    delegate:delegate()] autorelease];
  }

 private:
  base::scoped_nsobject<CredentialItemTestDelegate> delegate_;
};

TEST_F(CredentialItemViewTest, BasicCredential) {
  autofill::PasswordForm form(BasicCredential());
  CredentialItemView* item = view(form);

  EXPECT_NSEQ(base::SysUTF16ToNSString(form.username_value),
              [item usernameLabel].stringValue);
  EXPECT_EQ(nil, [item nameLabel]);
  EXPECT_FALSE([delegate() didFetchAvatar]);
  EXPECT_TRUE(
      ImagesEqual([CredentialItemView defaultAvatar], [item avatarView].image));
}

TEST_F(CredentialItemViewTest, CredentialWithName) {
  autofill::PasswordForm form(CredentialWithName());
  CredentialItemView* item = view(form);

  EXPECT_NSEQ(base::SysUTF16ToNSString(form.username_value),
              [item usernameLabel].stringValue);
  EXPECT_NSEQ(base::SysUTF16ToNSString(form.display_name),
              [item nameLabel].stringValue);
  EXPECT_FALSE([delegate() didFetchAvatar]);
  EXPECT_TRUE(
      ImagesEqual([CredentialItemView defaultAvatar], [item avatarView].image));
}

TEST_F(CredentialItemViewTest, CredentialWithAvatar) {
  autofill::PasswordForm form(CredentialWithAvatar());
  CredentialItemView* item = view(form);

  EXPECT_NSEQ(base::SysUTF16ToNSString(form.username_value),
              [item usernameLabel].stringValue);
  EXPECT_EQ(nil, [item nameLabel]);
  EXPECT_TRUE([delegate() didFetchAvatar]);
  EXPECT_EQ(form.avatar_url, [delegate() fetchedAvatarURL]);
  EXPECT_EQ(item, [delegate() viewForFetchedAvatar]);
  EXPECT_TRUE(
      ImagesEqual([CredentialItemView defaultAvatar], [item avatarView].image));

  [item updateAvatar:nil];
  EXPECT_FALSE([item avatarView].image);
}

TEST_F(CredentialItemViewTest, CredentialWithNameAndAvatar) {
  autofill::PasswordForm form(CredentialWithNameAndAvatar());
  CredentialItemView* item = view(form);

  EXPECT_NSEQ(base::SysUTF16ToNSString(form.username_value),
              [item usernameLabel].stringValue);
  EXPECT_NSEQ(base::SysUTF16ToNSString(form.display_name),
              [item nameLabel].stringValue);
  EXPECT_TRUE([delegate() didFetchAvatar]);
  EXPECT_EQ(form.avatar_url, [delegate() fetchedAvatarURL]);
  EXPECT_EQ(item, [delegate() viewForFetchedAvatar]);
  EXPECT_TRUE(
      ImagesEqual([CredentialItemView defaultAvatar], [item avatarView].image));

  [item updateAvatar:nil];
  EXPECT_FALSE([item avatarView].image);
}

}  // namespace
