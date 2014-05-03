// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_sign_in_delegate.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"

namespace {

// Platform version of the sign-in delegate, allows handling of hotkeys.
class CocoaSignInDelegate : public autofill::AutofillDialogSignInDelegate {
 public:
  CocoaSignInDelegate(autofill::AutofillDialogView* dialog_view,
                      content::WebContents* dialog_web_contents,
                      content::WebContents* originating_web_contents,
                      const gfx::Size& minimum_size,
                      const gfx::Size& maximum_size,
                      NSView* view)
      : AutofillDialogSignInDelegate(dialog_view,
                                     dialog_web_contents,
                                     originating_web_contents,
                                     minimum_size,
                                     maximum_size),
        view_(view) {}

  // WebContentsDelegate implementation. Forwards all unhandled keyboard events
  // to the current window.
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

 private:
  NSView* view_;  // WebContentsView, used to redispatch key events.
};

void CocoaSignInDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return;

  // Not invoking +[BrowserWindowUtils handleKeyboardEvent here], since the
  // window in question is a ConstrainedWindowCustomWindow, not a BrowserWindow.
  ChromeEventProcessingWindow* event_window =
      base::mac::ObjCCastStrict<ChromeEventProcessingWindow>([view_ window]);
  [event_window redispatchKeyEvent:event.os_event];
}

}  // namespace

@implementation AutofillSignInContainer

@synthesize preferredSize = preferredSize_;

- (id)initWithDialog:(autofill::AutofillDialogCocoa*)dialog {
  if (self = [super init]) {
    dialog_ = dialog;
  }
  return self;
}

- (void)loadView {
  webContents_.reset(
      content::WebContents::Create(
          content::WebContents::CreateParams(dialog_->delegate()->profile())));
  NSView* webContentView = webContents_->GetNativeView();
  [self setView:webContentView];
}

- (void)loadSignInPage {
  DCHECK(webContents_.get());

  // Ensure initial minimum size doesn't cause resize.
  NSSize initialMinSize = [[self view] frame].size;

  // Ensure |maxSize_| is bigger than |initialMinSize|.
  maxSize_.height = std::max(maxSize_.height, initialMinSize.height);
  maxSize_.width = std::max(maxSize_.width, initialMinSize.width);

  signInDelegate_.reset(
      new CocoaSignInDelegate(
          dialog_,
          webContents_.get(),
          dialog_->delegate()->GetWebContents(),
          gfx::Size(NSSizeToCGSize(initialMinSize)),
          gfx::Size(NSSizeToCGSize(maxSize_)),
          [self view]));
  webContents_->GetController().LoadURL(
      dialog_->delegate()->SignInUrl(),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

- (content::NavigationController*)navigationController {
  return &webContents_->GetController();
}

- (content::WebContents*)webContents {
  return webContents_.get();
}

- (void)constrainSizeToMinimum:(NSSize)minSize maximum:(NSSize)maxSize {
  minSize_ = minSize;
  maxSize_ = maxSize;

  // Constrain the web view to be a little shorter than the given sizes, leaving
  // room for some padding below the web view.
  minSize_.height -= chrome_style::kClientBottomPadding;
  maxSize_.height -= chrome_style::kClientBottomPadding;

  // Notify the web contents of its new auto-resize limits.
  if (signInDelegate_ && ![[self view] isHidden]) {
    signInDelegate_->UpdateLimitsAndEnableAutoResize(
        gfx::Size(NSSizeToCGSize(minSize_)),
        gfx::Size(NSSizeToCGSize(maxSize_)));
  }
}

- (void)setPreferredSize:(NSSize)size {
  // The |size| is the preferred size requested by the web view. Tack onto that
  // a bit of extra padding at the bottom.
  preferredSize_ = size;
  preferredSize_.height += chrome_style::kClientBottomPadding;

  // Always request re-layout if preferredSize changes.
  id delegate = [[[self view] window] windowController];
  if ([delegate respondsToSelector:@selector(requestRelayout)])
    [delegate performSelector:@selector(requestRelayout)];
}

@end
