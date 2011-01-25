// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bug_report_window_controller.h"

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bug_report_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"

@implementation BugReportWindowController

@synthesize bugDescription = bugDescription_;
@synthesize bugTypeIndex = bugTypeIndex_;
@synthesize pageURL = pageURL_;
@synthesize pageTitle = pageTitle_;
@synthesize sendScreenshot = sendScreenshot_;
@synthesize disableScreenshotCheckbox = disableScreenshotCheckbox_;
@synthesize bugTypeList = bugTypeList_;

- (id)initWithTabContents:(TabContents*)currentTab
                  profile:(Profile*)profile {
  NSString* nibpath = [base::mac::MainAppBundle() pathForResource:@"ReportBug"
                                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    currentTab_ = currentTab;
    profile_ = profile;

    // The order of strings in this array must match the order of the bug types
    // declared below in the bugTypeFromIndex function.
    bugTypeList_ = [[NSMutableArray alloc] initWithObjects:
        l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_CHROME_MISBEHAVES),
        l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SOMETHING_MISSING),
        l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_BROWSER_CRASH),
        l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_OTHER_PROBLEM),
        nil];

    if (currentTab_ != NULL) {
      // Get data from current tab, if one exists. This dialog could be called
      // from the main menu with no tab contents, so currentTab_ is not
      // guaranteed to be non-NULL.
      // TODO(mirandac): This dialog should be a tab-modal sheet if a browser
      // window exists.
      [self setSendScreenshot:YES];
      [self setDisableScreenshotCheckbox:NO];
      // Insert menu items about bugs related to specific pages.
      [bugTypeList_ insertObjects:
          [NSArray arrayWithObjects:
              l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PAGE_WONT_LOAD),
              l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PAGE_LOOKS_ODD),
              l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_PHISHING_PAGE),
              l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_CANT_SIGN_IN),
              nil]
          atIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, 4)]];

      [self setPageURL:base::SysUTF8ToNSString(
          currentTab_->controller().GetActiveEntry()->url().spec())];
      [self setPageTitle:base::SysUTF16ToNSString(currentTab_->GetTitle())];
      base::mac::GrabWindowSnapshot(
          currentTab_->view()->GetTopLevelNativeWindow(), &pngData_,
          &pngWidth_, &pngHeight_);
    } else {
      // If no current tab exists, create a menu without the "broken page"
      // options, with page URL and title empty, and screenshot disabled.
      [self setSendScreenshot:NO];
      [self setDisableScreenshotCheckbox:YES];
    }

    pngHeight_ = 0;
    pngWidth_ = 0;
  }
  return self;
}

- (void)dealloc {
  [pageURL_ release];
  [pageTitle_ release];
  [bugDescription_ release];
  [bugTypeList_ release];
  [bugTypeDictionary_ release];
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
        [self bugTypeFromIndex],
        base::SysNSStringToUTF8(pageURL_),
        base::SysNSStringToUTF8(bugDescription_),
        sendScreenshot_ && !pngData_.empty() ?
            reinterpret_cast<const char *>(&(pngData_[0])) : NULL,
        pngData_.size(), pngWidth_, pngHeight_);
  }
  [self closeDialog];
}

- (IBAction)cancel:(id)sender {
  [self closeDialog];
}

- (BOOL)isPhishingReport {
  return [self bugTypeFromIndex] == BugReportUtil::PHISHING_PAGE;
}

- (int)bugTypeFromIndex {
  // The order of these bugs must match the ordering in the bugTypeList_,
  // and thereby the menu in the popup button in the dialog box.
  const BugReportUtil::BugType typesForMenuIndices[] = {
    BugReportUtil::PAGE_WONT_LOAD,
    BugReportUtil::PAGE_LOOKS_ODD,
    BugReportUtil::PHISHING_PAGE,
    BugReportUtil::CANT_SIGN_IN,
    BugReportUtil::CHROME_MISBEHAVES,
    BugReportUtil::SOMETHING_MISSING,
    BugReportUtil::BROWSER_CRASH,
    BugReportUtil::OTHER_PROBLEM
  };
  // The bugs for the shorter menu start at index 4.
  NSUInteger adjustedBugTypeIndex_ = [bugTypeList_ count] == 8 ? bugTypeIndex_ :
      bugTypeIndex_ + 4;
  DCHECK_LT(adjustedBugTypeIndex_, arraysize(typesForMenuIndices));
  return typesForMenuIndices[adjustedBugTypeIndex_];
}

// Custom setter to update the UI for different bug types.
- (void)setBugTypeIndex:(NSUInteger)bugTypeIndex {
  bugTypeIndex_ = bugTypeIndex;

  // The "send" button's title is based on the type of report.
  NSString* buttonTitle = [self isPhishingReport] ?
      l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SEND_PHISHING_REPORT) :
      l10n_util::GetNSStringWithFixup(IDS_BUGREPORT_SEND_REPORT);
  if (![buttonTitle isEqualTo:[sendReportButton_ title]]) {
    NSRect sendFrame1 = [sendReportButton_ frame];
    NSRect cancelFrame1 = [cancelButton_ frame];

    [sendReportButton_ setTitle:buttonTitle];
    CGFloat deltaWidth =
        [GTMUILocalizerAndLayoutTweaker sizeToFitView:sendReportButton_].width;

    NSRect sendFrame2 = [sendReportButton_ frame];
    sendFrame2.origin.x -= deltaWidth;
    NSRect cancelFrame2 = cancelFrame1;
    cancelFrame2.origin.x -= deltaWidth;

    // Since the buttons get updated/resize, use a quick animation so it is
    // a little less jarring in the UI.
    NSDictionary* sendReportButtonResize =
        [NSDictionary dictionaryWithObjectsAndKeys:
         sendReportButton_, NSViewAnimationTargetKey,
         [NSValue valueWithRect:sendFrame1], NSViewAnimationStartFrameKey,
         [NSValue valueWithRect:sendFrame2], NSViewAnimationEndFrameKey,
         nil];
    NSDictionary* cancelButtonResize =
        [NSDictionary dictionaryWithObjectsAndKeys:
         cancelButton_, NSViewAnimationTargetKey,
         [NSValue valueWithRect:cancelFrame1], NSViewAnimationStartFrameKey,
         [NSValue valueWithRect:cancelFrame2], NSViewAnimationEndFrameKey,
         nil];
    NSAnimation* animation =
        [[[NSViewAnimation alloc] initWithViewAnimations:
           [NSArray arrayWithObjects:sendReportButtonResize, cancelButtonResize,
             nil]] autorelease];
    const NSTimeInterval kQuickTransitionInterval = 0.1;
    [animation setDuration:kQuickTransitionInterval];
    [animation startAnimation];

    // Save or reload description when moving between phishing page and other
    // bug report types.
    if ([self isPhishingReport]) {
        saveBugDescription_.reset([[self bugDescription] retain]);
        [self setBugDescription:nil];
        saveSendScreenshot_ = sendScreenshot_;
        [self setSendScreenshot:NO];
    } else {
        [self setBugDescription:saveBugDescription_.get()];
        saveBugDescription_.reset();
        [self setSendScreenshot:saveSendScreenshot_];
    }
  }
}

- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView
    doCommandBySelector:(SEL)commandSelector {
  if (commandSelector == @selector(insertNewline:)) {
    [textView insertNewlineIgnoringFieldEditor:self];
    return YES;
  }
  return NO;
}

// BugReportWindowController needs to change the title of the Send Report
// button when the user chooses the phishing bug type, so we need to bind
// the function that changes the button title to the bug type key.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* paths = [super keyPathsForValuesAffectingValueForKey:key];
  if ([key isEqualToString:@"isPhishingReport"]) {
    paths = [paths setByAddingObject:@"bugTypeIndex"];
  }
  return paths;
}

@end

