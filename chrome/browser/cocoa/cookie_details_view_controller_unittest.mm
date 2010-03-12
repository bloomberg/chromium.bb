// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/cookie_details_view_controller.h"
#include "chrome/browser/cookie_modal_dialog.h"

// Mock apapter that returns dummy values for the details view
@interface MockCookieDetailsViewContentAdapter : NSObject {
 @private
  // The type of fake cookie data to display
  CookiePromptModalDialog::DialogType promptType_;
}

- (id)initWithType:(CookiePromptModalDialog::DialogType)type;

// The following methods are all used in the bindings
// inside the cookie detail view.
@property (readonly) BOOL isFolderOrCookieTreeDetails;
@property (readonly) BOOL isLocalStorageTreeDetails;
@property (readonly) BOOL isDatabaseTreeDetails;
@property (readonly) BOOL isDatabasePromptDetails;
@property (readonly) BOOL isLocalStoragePromptDetails;
@property (readonly) NSString* name;
@property (readonly) NSString* content;
@property (readonly) NSString* domain;
@property (readonly) NSString* path;
@property (readonly) NSString* sendFor;
@property (readonly) NSString* created;
@property (readonly) NSString* expires;
@property (readonly) NSString* fileSize;
@property (readonly) NSString* lastModified;
@property (readonly) NSString* databaseDescription;
@property (readonly) NSString* localStorageKey;
@property (readonly) NSString* localStorageValue;

@end

@implementation MockCookieDetailsViewContentAdapter

- (id)initWithType:(CookiePromptModalDialog::DialogType)type {
  if ((self = [super init])) {
    promptType_ = type;
  }
  return self;
}

- (BOOL)isFolderOrCookieTreeDetails {
  return promptType_ == CookiePromptModalDialog::DIALOG_TYPE_COOKIE;
}

- (BOOL)isLocalStorageTreeDetails {
  return false;
}

- (BOOL)isDatabaseTreeDetails {
  return false;
}

- (BOOL) isDatabasePromptDetails {
  return promptType_ == CookiePromptModalDialog::DIALOG_TYPE_DATABASE;
}

- (BOOL) isLocalStoragePromptDetails {
  return promptType_ == CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE;
}

- (NSString*)name {
    return @"dummyName";
}

- (NSString*)content {
  return @"dummyContent";
}

- (NSString*)domain {
  return @"dummyDomain";
}

- (NSString*)path {
  return @"dummyPath";
}

- (NSString*)sendFor {
  return @"dummySendFor";
}

- (NSString*)created {
  return @"dummyCreated";
}

- (NSString*)expires {
  return @"dummyExpires";
}

- (NSString*)fileSize {
  return @"dummyFileSize";
}

- (NSString*)lastModified {
  return @"dummyLastModified";
}

- (NSString*)databaseDescription {
  return @"dummyDatabaseDescription";
}

- (NSString*)localStorageKey {
  return @"dummyLocalStorageKey";
}

- (NSString*)localStorageValue {
  return @"dummyLocalStorageValue";
}

@end

namespace {

class CookieDetailsViewControllerTest : public CocoaTest {
};

TEST_F(CookieDetailsViewControllerTest, Create) {
  scoped_nsobject<CookieDetailsViewController> detailsViewController;
  detailsViewController.reset([[CookieDetailsViewController alloc] init]);
}

TEST_F(CookieDetailsViewControllerTest, ShrinkToFit) {
  scoped_nsobject<CookieDetailsViewController> detailsViewController;
  detailsViewController.reset([[CookieDetailsViewController alloc] init]);
  scoped_nsobject<MockCookieDetailsViewContentAdapter> mockAdapter;
  mockAdapter.reset([[MockCookieDetailsViewContentAdapter alloc]
      initWithType:CookiePromptModalDialog::DIALOG_TYPE_DATABASE]);
  [detailsViewController.get() setContentObject:mockAdapter.get()];
  NSRect beforeFrame = [[detailsViewController.get() view] frame];
  [detailsViewController.get() shrinkViewToFit];
  NSRect afterFrame = [[detailsViewController.get() view] frame];
  EXPECT_TRUE(afterFrame.size.height < beforeFrame.size.width);
}

}  // namespace
