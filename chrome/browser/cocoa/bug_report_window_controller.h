// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BUG_REPORT_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BUG_REPORT_WINDOW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/scoped_ptr.h"

class Profile;
class TabContents;

// A window controller for managing the "Report Bug" feature. Modally
// presents a dialog that allows the user to either file a bug report on
// a broken page, or go directly to Google's "Report Phishing" page and
// file a report there.
@interface BugReportWindowController : NSWindowController {
 @private
  TabContents* currentTab_;  // Weak, owned by browser.
  Profile* profile_;  // Weak, owned by browser.

  // Holds screenshot of current tab.
  std::vector<unsigned char> pngData_;

  // Values bound to data in the dialog box. These values cannot be boxed in
  // scoped_nsobjects because we use them for bindings.
  NSString* bugDescription_;  // Strong.
  NSUInteger bugType_;
  NSString* pageTitle_;  // Strong.
  NSString* pageURL_;  // Strong.

  // We keep a pointer to this button so we can change its title.
  NSButton* sendReportButton_;  // Weak.

  BOOL sendScreenshot_;

  // Disable screenshot if no browser window is open.
  BOOL disableScreenshot_;

  // Menu for the bug type popup button.  We create it here instead of in
  // IB so that we can nicely check whether the phishing page is selected,
  // and so that we can create a menu without "page" options when no browser
  // window is open.
  NSArray* bugTypeList_;  // Strong.
}

// Initialize with the contents of the tab to be reported as buggy / wrong.
// If dialog is called without an open window, currentTab may be null; in
// that case, a dialog is opened with options for reporting a bugs not
// related to a specific page.  Profile is passed to BugReportUtil, who
// will not send a report if the value is null.
- (id)initWithTabContents:(TabContents*)currentTab profile:(Profile*)profile;

// Run the dialog with an application-modal event loop.  If the user accepts,
// send the report of the bug or broken web site.
- (void)runModalDialog;

// IBActions for the dialog buttons.
- (IBAction)sendReport:(id)sender;
- (IBAction)cancel:(id)sender;

// YES if the user has selected the phishing report option.
- (BOOL)isPhishingReport;

// The "send report" button may need to change its title to reflect that it's
// bouncing to the phish report page instead of sending a report directly
// from the dialog box (or vice versa). Observe the menu of bug types
// and change the button title along with the selected bug.
- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item;

// Properties for bindings.
@property (copy, nonatomic) NSString* bugDescription;
@property NSUInteger bugType;
@property (copy, nonatomic) NSString* pageTitle;
@property (copy, nonatomic) NSString* pageURL;
@property (assign, nonatomic) IBOutlet NSButton* sendReportButton;
@property BOOL sendScreenshot;
@property BOOL disableScreenshot;
@property (readonly, nonatomic) NSArray* bugTypeList;

@end

#endif  // CHROME_BROWSER_COCOA_BUG_REPORT_WINDOW_CONTROLLER_H_

