// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/ref_counted.h"
#import "chrome/browser/cocoa/bug_report_window_controller.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/browser/profile.h"

namespace {

class BugReportWindowControllerUnittest : public RenderViewHostTestHarness {
};

// See http://crbug.com/29019 for why it's disabled.
TEST_F(BugReportWindowControllerUnittest, DISABLED_ReportBugWithNewTabPageOpen) {
  ChromeThread ui_thread(ChromeThread::UI, MessageLoop::current());
  // Create a "chrome://newtab" test tab.  SiteInstance will be deleted when
  // tabContents is deleted.
  SiteInstance* instance =
      SiteInstance::CreateSiteInstance(profile_.get());
  TestTabContents* tabContents = new TestTabContents(profile_.get(),
                                                      instance);
  tabContents->controller().LoadURL(GURL("chrome://newtab"),
      GURL(), PageTransition::START_PAGE);

  BugReportWindowController* controller = [[BugReportWindowController alloc]
      initWithTabContents:tabContents
                  profile:profile_.get()];

  // The phishing report bug is stored at index 2 in the Report Bug dialog.
  [controller setBugTypeIndex:2];
  EXPECT_TRUE([controller isPhishingReport]);
  [controller setBugTypeIndex:1];
  EXPECT_FALSE([controller isPhishingReport]);

  // Make sure that the tab was correctly recorded.
  EXPECT_TRUE([[controller pageURL] isEqualToString:@"chrome://newtab/"]);
  EXPECT_TRUE([[controller pageTitle] isEqualToString:@"New Tab"]);

  // When we call "report bug" with non-empty tab contents, all menu options
  // should be available, and we should send screenshot by default.
  EXPECT_EQ([[controller bugTypeList] count], 8U);
  EXPECT_TRUE([controller sendScreenshot]);

  delete tabContents;
  [controller release];
}

// See http://crbug.com/29019 for why it's disabled.
TEST_F(BugReportWindowControllerUnittest, DISABLED_ReportBugWithNoWindowOpen) {
  BugReportWindowController* controller = [[BugReportWindowController alloc]
      initWithTabContents:NULL
                  profile:profile_.get()];

  // Make sure that no page title or URL are recorded. Note that IB reports
  // empty textfields as NULL values.
  EXPECT_FALSE([controller pageURL]);
  EXPECT_FALSE([controller pageTitle]);

  // When we call "report bug" with empty tab contents, only menu options
  // that don't refer to a specific page should be available, and the send
  // screenshot option should be turned off.
  EXPECT_EQ([[controller bugTypeList] count], 4U);
  EXPECT_FALSE([controller sendScreenshot]);

  [controller release];
}

}  // namespace

