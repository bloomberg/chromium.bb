// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using extensions::Extension;

// Base class for our tests.
class ExtensionInstallViewControllerTest : public CocoaProfileTest {
 public:
  ExtensionInstallViewControllerTest() {
    extension_ = chrome::LoadInstallPromptExtension();
  }

 protected:
  scoped_refptr<extensions::Extension> extension_;
};

// Test that we can load the two kinds of prompts correctly, that the outlets
// are hooked up, and that the dialog calls cancel when cancel is pressed.
TEST_F(ExtensionInstallViewControllerTest, BasicsNormalCancel) {
  chrome::MockExtensionInstallPromptDelegate delegate;

  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension_.get());

  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  prompt.SetPermissions(permissions);

  scoped_nsobject<ExtensionInstallViewController>
    controller([[ExtensionInstallViewController alloc]
                 initWithNavigator:browser()
                          delegate:&delegate
                            prompt:prompt]);

  [controller view];  // Force nib load.

  // Test the right nib loaded.
  EXPECT_NSEQ(@"ExtensionInstallPrompt", [controller nibName]);

  // Check all the controls.
  // Make sure everything is non-nil, and that the fields that are
  // auto-translated don't start with a caret (that would indicate that they
  // were not translated).
  EXPECT_TRUE([controller iconView]);
  EXPECT_TRUE([[controller iconView] image]);

  EXPECT_TRUE([controller titleField]);
  EXPECT_NE(0u, [[[controller titleField] stringValue] length]);

  NSOutlineView* outlineView = [controller outlineView];
  EXPECT_TRUE(outlineView);
  EXPECT_EQ(2, [outlineView numberOfRows]);
  EXPECT_NSEQ([[outlineView dataSource] outlineView:outlineView
                          objectValueForTableColumn:nil
                                             byItem:[outlineView itemAtRow:1]],
              base::SysUTF16ToNSString(prompt.GetPermission(0)));

  EXPECT_TRUE([controller cancelButton]);
  EXPECT_NE(0u, [[[controller cancelButton] stringValue] length]);
  EXPECT_NE('^', [[[controller cancelButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller okButton]);
  EXPECT_NE(0u, [[[controller okButton] stringValue] length]);
  EXPECT_NE('^', [[[controller okButton] stringValue] characterAtIndex:0]);

  // Test that cancel calls our delegate.
  [controller cancel:nil];
  EXPECT_EQ(1, delegate.abort_count());
  EXPECT_EQ(0, delegate.proceed_count());
}


TEST_F(ExtensionInstallViewControllerTest, BasicsNormalOK) {
  chrome::MockExtensionInstallPromptDelegate delegate;

  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension_.get());
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  prompt.SetPermissions(permissions);

  scoped_nsobject<ExtensionInstallViewController>
  controller([[ExtensionInstallViewController alloc]
               initWithNavigator:browser()
                        delegate:&delegate
                          prompt:prompt]);

  [controller view];  // Force nib load.
  [controller ok:nil];

  EXPECT_EQ(0, delegate.abort_count());
  EXPECT_EQ(1, delegate.proceed_count());
}

// Test that controls get repositioned when there are two warnings vs one
// warning.
TEST_F(ExtensionInstallViewControllerTest, MultipleWarnings) {
  chrome::MockExtensionInstallPromptDelegate delegate1;
  chrome::MockExtensionInstallPromptDelegate delegate2;

  ExtensionInstallPrompt::Prompt one_warning_prompt =
      chrome::BuildExtensionInstallPrompt(extension_.get());
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  one_warning_prompt.SetPermissions(permissions);

  ExtensionInstallPrompt::Prompt two_warnings_prompt =
      chrome::BuildExtensionInstallPrompt(extension_.get());
  permissions.push_back(UTF8ToUTF16("warning 2"));
  two_warnings_prompt.SetPermissions(permissions);

  scoped_nsobject<ExtensionInstallViewController>
  controller1([[ExtensionInstallViewController alloc]
                initWithNavigator:browser()
                         delegate:&delegate1
                           prompt:one_warning_prompt]);

  [controller1 view];  // Force nib load.

  scoped_nsobject<ExtensionInstallViewController>
  controller2([[ExtensionInstallViewController alloc]
                initWithNavigator:browser()
                         delegate:&delegate2
                           prompt:two_warnings_prompt]);

  [controller2 view];  // Force nib load.

  // Test control positioning. We don't test exact positioning because we don't
  // want this to depend on string details and localization. But we do know the
  // relative effect that adding a second warning should have on the layout.
  ASSERT_LT([[controller1 view] frame].size.height,
            [[controller2 view] frame].size.height);

  ASSERT_LT([[controller1 view] frame].size.height,
            [[controller2 view] frame].size.height);
}

// Test that we can load the skinny prompt correctly, and that the outlets are
// are hooked up.
TEST_F(ExtensionInstallViewControllerTest, BasicsSkinny) {
  chrome::MockExtensionInstallPromptDelegate delegate;

  // No warnings should trigger skinny prompt.
  ExtensionInstallPrompt::Prompt no_warnings_prompt =
      chrome::BuildExtensionInstallPrompt(extension_.get());

  scoped_nsobject<ExtensionInstallViewController>
  controller([[ExtensionInstallViewController alloc]
               initWithNavigator:browser()
                        delegate:&delegate
                          prompt:no_warnings_prompt]);

  [controller view];  // Force nib load.

  // Test the right nib loaded.
  EXPECT_NSEQ(@"ExtensionInstallPromptNoWarnings", [controller nibName]);

  // Check all the controls.
  // In the skinny prompt, only the icon, title and buttons are non-nill.
  // Everything else is nil.
  EXPECT_TRUE([controller iconView]);
  EXPECT_TRUE([[controller iconView] image]);

  EXPECT_TRUE([controller titleField]);
  EXPECT_NE(0u, [[[controller titleField] stringValue] length]);

  EXPECT_TRUE([controller cancelButton]);
  EXPECT_NE(0u, [[[controller cancelButton] stringValue] length]);
  EXPECT_NE('^', [[[controller cancelButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller okButton]);
  EXPECT_NE(0u, [[[controller okButton] stringValue] length]);
  EXPECT_NE('^', [[[controller okButton] stringValue] characterAtIndex:0]);

  EXPECT_FALSE([controller outlineView]);
}


// Test that we can load the inline prompt correctly, and that the outlets are
// are hooked up.
TEST_F(ExtensionInstallViewControllerTest, BasicsInline) {
  chrome::MockExtensionInstallPromptDelegate delegate;

  // No warnings should trigger skinny prompt.
  ExtensionInstallPrompt::Prompt inline_prompt(
      ExtensionInstallPrompt::INLINE_INSTALL_PROMPT);
  inline_prompt.SetInlineInstallWebstoreData("1,000", 3.5, 200);
  inline_prompt.set_extension(extension_.get());
  inline_prompt.set_icon(chrome::LoadInstallPromptIcon());

  scoped_nsobject<ExtensionInstallViewController>
  controller([[ExtensionInstallViewController alloc]
               initWithNavigator:browser()
                        delegate:&delegate
                          prompt:inline_prompt]);

  [controller view];  // Force nib load.

  // Test the right nib loaded.
  EXPECT_NSEQ(@"ExtensionInstallPromptInline", [controller nibName]);

  // Check all the controls.
  EXPECT_TRUE([controller iconView]);
  EXPECT_TRUE([[controller iconView] image]);

  EXPECT_TRUE([controller titleField]);
  EXPECT_NE(0u, [[[controller titleField] stringValue] length]);

  EXPECT_TRUE([controller cancelButton]);
  EXPECT_NE(0u, [[[controller cancelButton] stringValue] length]);
  EXPECT_NE('^', [[[controller cancelButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller okButton]);
  EXPECT_NE(0u, [[[controller okButton] stringValue] length]);
  EXPECT_NE('^', [[[controller okButton] stringValue] characterAtIndex:0]);

  EXPECT_TRUE([controller ratingStars]);
  EXPECT_EQ(5u, [[[controller ratingStars] subviews] count]);

  EXPECT_TRUE([controller ratingCountField]);
  EXPECT_NE(0u, [[[controller ratingCountField] stringValue] length]);

  EXPECT_TRUE([controller userCountField]);
  EXPECT_NE(0u, [[[controller userCountField] stringValue] length]);

  EXPECT_TRUE([controller storeLinkButton]);
  EXPECT_NE(0u, [[[controller storeLinkButton] stringValue] length]);
  EXPECT_NE('^',
            [[[controller storeLinkButton] stringValue] characterAtIndex:0]);

  // Though we have no permissions warnings, these should still be hooked up,
  // just invisible.
  EXPECT_TRUE([controller outlineView]);
  EXPECT_TRUE([[[controller outlineView] enclosingScrollView] isHidden]);
  EXPECT_TRUE([controller warningsSeparator]);
  EXPECT_TRUE([[controller warningsSeparator] isHidden]);
}

TEST_F(ExtensionInstallViewControllerTest, OAuthIssues) {
  chrome::MockExtensionInstallPromptDelegate delegate;

  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension_);
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  prompt.SetPermissions(permissions);
  IssueAdviceInfoEntry issue;
  issue.description = UTF8ToUTF16("issue description 1");
  issue.details.push_back(UTF8ToUTF16("issue detail 1"));
  IssueAdviceInfo issues;
  issues.push_back(issue);
  prompt.SetOAuthIssueAdvice(issues);

  scoped_nsobject<ExtensionInstallViewController>
  controller([[ExtensionInstallViewController alloc]
               initWithNavigator:browser()
                        delegate:&delegate
                          prompt:prompt]);

  [controller view];  // Force nib load.
  NSOutlineView* outlineView = [controller outlineView];
  EXPECT_TRUE(outlineView);
  EXPECT_EQ(4, [outlineView numberOfRows]);
  EXPECT_NSEQ([[outlineView dataSource] outlineView:outlineView
                          objectValueForTableColumn:nil
                                             byItem:[outlineView itemAtRow:3]],
              base::SysUTF16ToNSString(prompt.GetOAuthIssue(0).description));
}

TEST_F(ExtensionInstallViewControllerTest, PostInstallPermissionsPrompt) {
  chrome::MockExtensionInstallPromptDelegate delegate;

  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionPostInstallPermissionsPrompt(extension_.get());
  std::vector<string16> permissions;
  permissions.push_back(UTF8ToUTF16("warning 1"));
  prompt.SetPermissions(permissions);

  scoped_nsobject<ExtensionInstallViewController>
  controller([[ExtensionInstallViewController alloc]
               initWithNavigator:browser()
                        delegate:&delegate
                          prompt:prompt]);

  [controller view];  // Force nib load.

  EXPECT_TRUE([controller cancelButton]);
  EXPECT_FALSE([controller okButton]);

  [controller cancel:nil];
  EXPECT_EQ(1, delegate.abort_count());
}
