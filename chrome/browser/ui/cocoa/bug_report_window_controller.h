// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BUG_REPORT_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BUG_REPORT_WINDOW_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/memory/scoped_nsobject.h"

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
  // Width and height of the current tab's screenshot.
  int pngWidth_;
  int pngHeight_;

  // Values bound to data in the dialog box. These values cannot be boxed in
  // scoped_nsobjects because we use them for bindings.
  NSString* bugDescription_;  // Strong.
  NSUInteger bugTypeIndex_;
  NSString* pageTitle_;  // Strong.
  NSString* pageURL_;  // Strong.

  // We keep a pointer to this button so we can change its title.
  IBOutlet NSButton* sendReportButton_;

  // This button must be moved when the send report button changes title.
  IBOutlet NSButton* cancelButton_;

  // The popup button that allows choice of bug type.
  IBOutlet NSPopUpButton* bugTypePopUpButton_;

  // YES sends a screenshot along with the bug report.
  BOOL sendScreenshot_;

  // Disable screenshot if no browser window is open.
  BOOL disableScreenshotCheckbox_;

  // Menu for the bug type popup button.  We create it here instead of in
  // IB so that we can nicely check whether the phishing page is selected,
  // and so that we can create a menu without "page" options when no browser
  // window is open.
  NSMutableArray* bugTypeList_;  // Strong.

  // When dialog switches from regular bug reports to phishing page, "save
  // screenshot" and "description" are disabled. Save the state of this value
  // to restore if the user switches back to a regular bug report before
  // sending.
  BOOL saveSendScreenshot_;
  scoped_nsobject<NSString> saveBugDescription_;  // Strong

  // Maps bug type menu item title strings to BugReportUtil::BugType ints.
  NSDictionary* bugTypeDictionary_;  // Strong
}

// Properties for bindings.
@property(nonatomic, copy) NSString* bugDescription;
@property(nonatomic) NSUInteger bugTypeIndex;
@property(nonatomic, copy) NSString* pageTitle;
@property(nonatomic, copy) NSString* pageURL;
@property(nonatomic) BOOL sendScreenshot;
@property(nonatomic) BOOL disableScreenshotCheckbox;
@property(nonatomic, readonly) NSArray* bugTypeList;

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

// Converts the bug type from the menu into the correct value for the bug type
// from BugReportUtil::BugType.
- (int)bugTypeFromIndex;

// Force the description text field to allow "return" to go to the next line
// within the description field. Without this delegate method, "return" falls
// back to the "Send Report" action, because this button has been bound to
// the return key in IB.
- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BUG_REPORT_WINDOW_CONTROLLER_H_
