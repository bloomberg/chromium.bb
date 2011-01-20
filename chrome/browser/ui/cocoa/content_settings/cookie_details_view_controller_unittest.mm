// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/content_settings/cookie_details.h"
#include "chrome/browser/ui/cocoa/content_settings/cookie_details_view_controller.h"

namespace {

class CookieDetailsViewControllerTest : public CocoaTest {
};

static CocoaCookieDetails* CreateTestCookieDetails(BOOL canEditExpiration) {
  GURL url("http://chromium.org");
  std::string cookieLine(
      "PHPSESSID=0123456789abcdef0123456789abcdef; path=/");
  net::CookieMonster::ParsedCookie pc(cookieLine);
  net::CookieMonster::CanonicalCookie cookie(url, pc);
  NSString* origin = base::SysUTF8ToNSString("http://chromium.org");
  CocoaCookieDetails* details = [CocoaCookieDetails alloc];
  [details initWithCookie:&cookie
                   origin:origin
        canEditExpiration:canEditExpiration];
  return [details autorelease];
}

static CookiePromptContentDetailsAdapter* CreateCookieTestContent(
    BOOL canEditExpiration) {
  CocoaCookieDetails* details = CreateTestCookieDetails(canEditExpiration);
  return [[[CookiePromptContentDetailsAdapter alloc] initWithDetails:details]
      autorelease];
}

static CocoaCookieDetails* CreateTestDatabaseDetails() {
  std::string domain("chromium.org");
  string16 name(base::SysNSStringToUTF16(@"wicked_name"));
  string16 desc(base::SysNSStringToUTF16(@"wicked_desc"));
  CocoaCookieDetails* details = [CocoaCookieDetails alloc];
  [details initWithDatabase:domain
               databaseName:name
        databaseDescription:desc
                   fileSize:2222];
  return [details autorelease];
}

static CookiePromptContentDetailsAdapter* CreateDatabaseTestContent() {
  CocoaCookieDetails* details = CreateTestDatabaseDetails();
  return [[[CookiePromptContentDetailsAdapter alloc] initWithDetails:details]
          autorelease];
}

TEST_F(CookieDetailsViewControllerTest, Create) {
  scoped_nsobject<CookieDetailsViewController> detailsViewController(
      [[CookieDetailsViewController alloc] init]);
}

TEST_F(CookieDetailsViewControllerTest, ShrinkToFit) {
  scoped_nsobject<CookieDetailsViewController> detailsViewController(
      [[CookieDetailsViewController alloc] init]);
  scoped_nsobject<CookiePromptContentDetailsAdapter> adapter(
      [CreateDatabaseTestContent() retain]);
  [detailsViewController.get() setContentObject:adapter.get()];
  NSRect beforeFrame = [[detailsViewController.get() view] frame];
  [detailsViewController.get() shrinkViewToFit];
  NSRect afterFrame = [[detailsViewController.get() view] frame];

  EXPECT_TRUE(afterFrame.size.height < beforeFrame.size.width);
}

TEST_F(CookieDetailsViewControllerTest, ExpirationEditability) {
  scoped_nsobject<CookieDetailsViewController> detailsViewController(
      [[CookieDetailsViewController alloc] init]);
  [detailsViewController view];
  scoped_nsobject<CookiePromptContentDetailsAdapter> adapter(
      [CreateCookieTestContent(YES) retain]);
  [detailsViewController.get() setContentObject:adapter.get()];

  EXPECT_FALSE([detailsViewController.get() hasExpiration]);
  [detailsViewController.get() setCookieHasExplicitExpiration:adapter.get()];
  EXPECT_TRUE([detailsViewController.get() hasExpiration]);
  [detailsViewController.get()
      setCookieDoesntHaveExplicitExpiration:adapter.get()];
  EXPECT_FALSE([detailsViewController.get() hasExpiration]);
}

}  // namespace
