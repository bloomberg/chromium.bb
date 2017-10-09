// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_mediator.h"

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_request.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ContextMenuDialogMediatorTest = PlatformTest;

// Tests context menu setup for a request with no URLs.
TEST_F(ContextMenuDialogMediatorTest, NoURLs) {
  // Create a context with |kMenuTitle|.
  NSString* kMenuTitle = @"Menu Title";
  web::ContextMenuParams params;
  params.menu_title.reset(kMenuTitle);
  ContextMenuDialogRequest* request =
      [ContextMenuDialogRequest requestWithParams:params];
  ContextMenuDialogMediator* mediator =
      [[ContextMenuDialogMediator alloc] initWithRequest:request];
  ASSERT_EQ(0U, [mediator textFieldConfigs].count);
  // Verify that the title was set.
  EXPECT_NSEQ(kMenuTitle, [mediator dialogTitle]);
  // Verify that the cancel button was added.
  EXPECT_EQ(1U, [mediator buttonConfigs].count);
  DialogButtonConfiguration* button = [mediator buttonConfigs][0];
  EXPECT_NSEQ(@"Cancel", button.text);
  EXPECT_EQ(DialogButtonStyle::CANCEL, button.style);
}

// Tests that the script button is shown for javacript: scheme URLs.
TEST_F(ContextMenuDialogMediatorTest, ScriptItem) {
  // Create a context with |kJavaScriptURL|.
  GURL kJavaScriptURL("javascript:alert('test');");
  web::ContextMenuParams params;
  params.link_url = kJavaScriptURL;
  ContextMenuDialogRequest* request =
      [ContextMenuDialogRequest requestWithParams:params];
  ContextMenuDialogMediator* mediator =
      [[ContextMenuDialogMediator alloc] initWithRequest:request];
  ASSERT_EQ(0U, [mediator textFieldConfigs].count);
  EXPECT_EQ(2U, [mediator buttonConfigs].count);
  DialogButtonConfiguration* button = [mediator buttonConfigs][0];
  EXPECT_NSEQ(@"Execute Script", button.text);
  EXPECT_EQ(DialogButtonStyle::DEFAULT, button.style);
}

// Tests that the link options are shown for valid link URLs.
TEST_F(ContextMenuDialogMediatorTest, LinkItems) {
  // Create a context with a valid link URL.
  web::ContextMenuParams params;
  params.link_url = GURL("http://valid.url.com");
  ContextMenuDialogRequest* request =
      [ContextMenuDialogRequest requestWithParams:params];
  ContextMenuDialogMediator* mediator =
      [[ContextMenuDialogMediator alloc] initWithRequest:request];
  ASSERT_EQ(0U, [mediator textFieldConfigs].count);
  NSArray<DialogButtonConfiguration*>* button_configs =
      [mediator buttonConfigs];
  NSArray* link_option_titles = @[
    @"Open In New Tab",
    @"Open In New Incognito Tab",
    @"Copy Link",
  ];
  EXPECT_EQ(link_option_titles.count + 1, button_configs.count);
  for (NSUInteger i = 0; i < link_option_titles.count; ++i) {
    EXPECT_NSEQ(link_option_titles[i], button_configs[i].text);
    EXPECT_EQ(DialogButtonStyle::DEFAULT, button_configs[i].style);
  }
}

// Tests that the image options are shown for valid image URLs.
TEST_F(ContextMenuDialogMediatorTest, ImageItems) {
  // Create a context with a valid image URL.
  web::ContextMenuParams params;
  params.src_url = GURL("http://valid.url.com");
  ContextMenuDialogRequest* request =
      [ContextMenuDialogRequest requestWithParams:params];
  ContextMenuDialogMediator* mediator =
      [[ContextMenuDialogMediator alloc] initWithRequest:request];
  ASSERT_EQ(0U, [mediator textFieldConfigs].count);
  NSArray<DialogButtonConfiguration*>* button_configs =
      [mediator buttonConfigs];
  NSArray* image_option_titles = @[
    @"Save Image",
    @"Open Image",
    @"Open Image In New Tab",
  ];
  EXPECT_EQ(image_option_titles.count + 1, button_configs.count);
  for (NSUInteger i = 0; i < image_option_titles.count; ++i) {
    EXPECT_NSEQ(image_option_titles[i], button_configs[i].text);
    EXPECT_EQ(DialogButtonStyle::DEFAULT, button_configs[i].style);
  }
}
