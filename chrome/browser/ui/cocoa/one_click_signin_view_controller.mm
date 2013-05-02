// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/one_click_signin_view_controller.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Shift the origin of |view|'s frame by the given amount in the
// positive y direction (up).
void ShiftOriginY(NSView* view, CGFloat amount) {
  NSPoint origin = [view frame].origin;
  origin.y += amount;
  [view setFrameOrigin:origin];
}

}  // namespace

@interface OneClickSigninViewController ()
- (CGFloat)initializeInformativeTextView;
- (void)close;
@end

@implementation OneClickSigninViewController


- (id)initWithNibName:(NSString*)nibName
          webContents:(content::WebContents*)webContents
         syncCallback:(const BrowserWindow::StartSyncCallback&)syncCallback
        closeCallback:(const base::Closure&)closeCallback
         isSyncDialog:(BOOL)isSyncDialog
         errorMessage:(NSString*)errorMessage {
  if ((self = [super initWithNibName:nibName
                              bundle:base::mac::FrameworkBundle()])) {
    webContents_ = webContents;
    startSyncCallback_ = syncCallback;
    closeCallback_ = closeCallback;
    isSyncDialog_ = isSyncDialog;
    clickedLearnMore_ = NO;
    errorMessage_.reset([errorMessage retain]);
    if (isSyncDialog_)
      DCHECK(!startSyncCallback_.is_null());
  }
  return self;
}

- (void)viewWillClose {
  // This is usually called after a click handler has initiated sync
  // and has reset the callback. However, in the case that we are closing
  // the window and nothing else has initiated the sync, we must do so here
  if (isSyncDialog_ && !startSyncCallback_.is_null()) {
    base::ResetAndReturn(&startSyncCallback_).Run(
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }
}

- (IBAction)ok:(id)sender {
  if (isSyncDialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        clickedLearnMore_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_OK :
            one_click_signin::HISTOGRAM_CONFIRM_OK);

    base::ResetAndReturn(&startSyncCallback_).Run(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }
  [self close];
}

- (IBAction)onClickUndo:(id)sender {
  if (isSyncDialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        clickedLearnMore_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_UNDO :
            one_click_signin::HISTOGRAM_CONFIRM_UNDO);

    base::ResetAndReturn(&startSyncCallback_).Run(
      OneClickSigninSyncStarter::UNDO_SYNC);

  }
  [self close];
}

- (IBAction)onClickAdvancedLink:(id)sender {
  if (isSyncDialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        clickedLearnMore_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED :
            one_click_signin::HISTOGRAM_CONFIRM_ADVANCED);

    base::ResetAndReturn(&startSyncCallback_).Run(
        OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
  }
  else {
    content::OpenURLParams params(GURL(chrome::kChromeUISettingsURL),
                                  content::Referrer(), CURRENT_TAB,
                                  content::PAGE_TRANSITION_LINK, false);
    webContents_->OpenURL(params);
  }
  [self close];
}

- (IBAction)onClickClose:(id)sender {
  if (isSyncDialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        clickedLearnMore_ ?
            one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE :
            one_click_signin::HISTOGRAM_CONFIRM_CLOSE);

    base::ResetAndReturn(&startSyncCallback_).Run(
        OneClickSigninSyncStarter::UNDO_SYNC);
  }
  [self close];
}

- (void)awakeFromNib {
  // Lay out the text controls from the bottom up.
  CGFloat totalYOffset = 0.0;

  totalYOffset +=
      [GTMUILocalizerAndLayoutTweaker sizeToFitView:advancedLink_].height;
  [[advancedLink_ cell] setTextColor:
      gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor())];

  if (informativePlaceholderTextField_) {
    ShiftOriginY(informativePlaceholderTextField_, totalYOffset);
    totalYOffset += [self initializeInformativeTextView];
  }

  ShiftOriginY(messageTextField_, totalYOffset);
  totalYOffset +=
      [GTMUILocalizerAndLayoutTweaker
          sizeToFitFixedWidthTextField:messageTextField_];

  if (closeButton_)
    ShiftOriginY(closeButton_, totalYOffset);

  NSSize delta = NSMakeSize(0.0, totalYOffset);

  if (!isSyncDialog_ && [errorMessage_ length] != 0)
    [messageTextField_ setStringValue:errorMessage_];

  // Resize bubble and window to hold the controls.
  [GTMUILocalizerAndLayoutTweaker
      resizeViewWithoutAutoResizingSubViews:[self view]
                                      delta:delta];

  if (isSyncDialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        one_click_signin::HISTOGRAM_CONFIRM_SHOWN);
  }
}

- (CGFloat)initializeInformativeTextView {
  NSRect oldFrame = [informativePlaceholderTextField_ frame];

  // Replace the placeholder NSTextField with the real label NSTextView. The
  // former doesn't show links in a nice way, but the latter can't be added in
  // a xib without a containing scroll view, so create the NSTextView
  // programmatically.
  informativeTextView_.reset(
      [[HyperlinkTextView alloc] initWithFrame:oldFrame]);
  [informativeTextView_.get() setAutoresizingMask:
      [informativePlaceholderTextField_ autoresizingMask]];
  [informativeTextView_.get() setDelegate:self];

  // Set the text.
  NSString* learnMoreText = l10n_util::GetNSStringWithFixup(IDS_LEARN_MORE);
  NSString* messageText;

  ui::ResourceBundle::FontStyle fontStyle = isSyncDialog_ ?
      chrome_style::kTextFontStyle : ui::ResourceBundle::SmallFont;
  NSFont* font = ui::ResourceBundle::GetSharedInstance().GetFont(
      fontStyle).GetNativeFont();

  // The non-modal bubble already has a text content and only needs the
  // Learn More link (in a smaller font).
  if (isSyncDialog_) {
    messageText = l10n_util::GetNSStringWithFixup(
      IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE);
    messageText = [messageText stringByAppendingString:@" "];
  } else {
    messageText = @"";
  }

  NSColor* linkColor =
      gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
  [informativeTextView_ setMessageAndLink:messageText
                                 withLink:learnMoreText
                                 atOffset:[messageText length]
                                     font:font
                             messageColor:[NSColor blackColor]
                                linkColor:linkColor];

  // Size to fit.
  [[informativePlaceholderTextField_ cell] setAttributedStringValue:
      [informativeTextView_ attributedString]];
  [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:informativePlaceholderTextField_];
  NSRect newFrame = [informativePlaceholderTextField_ frame];
  [informativeTextView_ setFrame:newFrame];

  // Swap placeholder.
  [[informativePlaceholderTextField_ superview]
     replaceSubview:informativePlaceholderTextField_
               with:informativeTextView_.get()];
  informativePlaceholderTextField_ = nil;  // Now released.

  return NSHeight(newFrame) - NSHeight(oldFrame);
}

- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if (isSyncDialog_ && !clickedLearnMore_) {
    clickedLearnMore_ = YES;

    OneClickSigninHelper::LogConfirmHistogramValue(
        one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE);
  }
  WindowOpenDisposition location = isSyncDialog_ ?
                                   NEW_WINDOW : NEW_FOREGROUND_TAB;
  content::OpenURLParams params(GURL(chrome::kChromeSyncLearnMoreURL),
                                content::Referrer(), location,
                                content::PAGE_TRANSITION_LINK, false);
  webContents_->OpenURL(params);
  return YES;
}

- (void)close {
  base::ResetAndReturn(&closeCallback_).Run();
}

@end
