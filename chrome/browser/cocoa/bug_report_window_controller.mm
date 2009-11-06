// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bug_report_window_controller.h"

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bug_report_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@implementation BugReportWindowController

@synthesize bugDescription = bugDescription_;
@synthesize bugType = bugType_;
@synthesize pageURL = pageURL_;
@synthesize pageTitle = pageTitle_;
@synthesize sendReportButton = sendReportButton_;
@synthesize cancelButton = cancelButton_;
@synthesize sendScreenshot = sendScreenshot_;
@synthesize disableScreenshot = disableScreenshot_;
@synthesize bugTypeList = bugTypeList_;

- (id)initWithTabContents:(TabContents*)currentTab
                  profile:(Profile*)profile {
  NSString* nibpath = [mac_util::MainAppBundle() pathForResource:@"ReportBug"
                                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    currentTab_ = currentTab;
    profile_ = profile;
    [self setBugDescription:@""];

    if (currentTab_ != NULL) {
      // Get data from current tab, if one exists. This dialog could be called
      // from the main menu with no tab contents, so currentTab_ is not
      // guaranteed to be non-NULL.
      // TODO(mirandac): This dialog should be a tab-modal sheet if a browser
      // window exists.
      [self setSendScreenshot:YES];
      [self setDisableScreenshot:NO];
      bugTypeList_ = [[NSArray alloc] initWithObjects:
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PAGE_WONT_LOAD),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PAGE_LOOKS_ODD),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PHISHING_PAGE),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_CANT_SIGN_IN),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_CHROME_MISBEHAVES),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SOMETHING_MISSING),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_BROWSER_CRASH),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_OTHER_PROBLEM),
          nil];
      [self setPageURL:base::SysUTF8ToNSString(
          currentTab_->controller().GetActiveEntry()->url().spec())];
      [self setPageTitle:base::SysUTF16ToNSString(currentTab_->GetTitle())];
      mac_util::GrabWindowSnapshot(
          currentTab_->view()->GetTopLevelNativeWindow(), &pngData_);
    } else {
      // If no current tab exists, create a menu without the "broken page"
      // options, with page URL and title empty, and screenshot disabled.
      [self setSendScreenshot:NO];
      [self setDisableScreenshot:YES];
      bugTypeList_ = [[NSArray alloc] initWithObjects:
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_CHROME_MISBEHAVES),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SOMETHING_MISSING),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_BROWSER_CRASH),
          l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_OTHER_PROBLEM),
          nil];
      // Because "Report Bug" is being called with no browser open in this
      // case, make URL and title empty.
      [self setPageURL:@""];
      [self setPageTitle:@""];
    }
  }
  return self;
}

- (void)dealloc {
  [pageURL_ release];
  [pageTitle_ release];
  [bugDescription_ release];
  [bugTypeList_ release];
  [super dealloc];
}

// Delegate callback so that closing the window deletes the controller.
- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

- (void)closeDialog {
  [NSApp stopModal];
  [[self window] close];
}

- (void)runModalDialog {
  NSWindow* bugReportWindow = [self window];
  [bugReportWindow center];
  [NSApp runModalForWindow:bugReportWindow];
}

- (IBAction)sendReport:(id)sender {
  if ([self isPhishingReport]) {
    BugReportUtil::ReportPhishing(currentTab_,
        pageURL_ ? base::SysNSStringToUTF8(pageURL_) : "");
  } else {
    BugReportUtil::SendReport(
        profile_,
        pageTitle_ ? base::SysNSStringToUTF8(pageTitle_) : "",
        bugType_,
        pageURL_ ? base::SysNSStringToUTF8(pageURL_) : "",
        bugDescription_ ? base::SysNSStringToUTF8(bugDescription_) : "",
        sendScreenshot_ && !pngData_.empty() ?
            reinterpret_cast<const char *>(&(pngData_[0])) : NULL,
        pngData_.size());
  }
  [self closeDialog];
}

- (IBAction)cancel:(id)sender {
  [self closeDialog];
}

- (BOOL)isPhishingReport {
  return bugType_ == [bugTypeList_ indexOfObject:
      l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PHISHING_PAGE)];
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item {
  NSString* buttonTitle = [[item title] isEqualToString:
      l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PHISHING_PAGE)] ?
      l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SEND_PHISHING_REPORT) :
      l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SEND_REPORT);
  if (![buttonTitle isEqualToString:[sendReportButton_ title]]) {
    [sendReportButton_ setTitle:buttonTitle];
    CGFloat deltaWidth =
        [GTMUILocalizerAndLayoutTweaker sizeToFitView:sendReportButton_].width;
    NSRect newSendButtonFrame = [sendReportButton_ frame];
    newSendButtonFrame.origin.x -= deltaWidth;
    NSRect newCancelButtonFrame = [cancelButton_ frame];
    newCancelButtonFrame.origin.x -= deltaWidth;
    [sendReportButton_ setFrame:newSendButtonFrame];
    [cancelButton_ setFrame:newCancelButtonFrame];
  }
}

// BugReportWindowController needs to change the title of the Send Report
// button when the user chooses the phishing bug type, so we need to bind
// the function that changes the button title to the bug type key.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* paths = [super keyPathsForValuesAffectingValueForKey:key];
  if ([key isEqualToString:@"isPhishingReport"]) {
    paths = [paths setByAddingObject:@"bugType"];
  }
  return paths;
}

@end


