// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/alternate_nav_infobar_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "webkit/glue/window_open_disposition.h"

@implementation AlternateNavInfoBarController

// Link infobars have a text message, of which part is linkified.  We
// use an NSAttributedString to display styled text, and we set a
// NSLink attribute on the hyperlink portion of the message.  Infobars
// use a custom NSTextField subclass, which allows us to override
// textView:clickedOnLink:atIndex: and intercept clicks.
//
- (void)addAdditionalControls {
  // No buttons.
  [self removeButtons];

  AlternateNavInfoBarDelegate* delegate =
      static_cast<AlternateNavInfoBarDelegate*>(delegate_);
  DCHECK(delegate);
  size_t offset = string16::npos;
  string16 message = delegate->GetMessageTextWithOffset(&offset);
  string16 link = delegate->GetLinkText();
  NSFont* font = [NSFont labelFontOfSize:
                  [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  HyperlinkTextView* view = (HyperlinkTextView*)label_.get();
  [view setMessageAndLink:base::SysUTF16ToNSString(message)
                 withLink:base::SysUTF16ToNSString(link)
                 atOffset:offset
                     font:font
             messageColor:[NSColor blackColor]
                linkColor:[NSColor blueColor]];
}

// Called when someone clicks on the link in the infobar.  This method
// is called by the InfobarTextField on its delegate (the
// AlternateNavInfoBarController).
- (void)linkClicked {
  if (![self isOwned])
    return;
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  AlternateNavInfoBarDelegate* delegate =
      static_cast<AlternateNavInfoBarDelegate*>(delegate_);
  if (delegate->LinkClicked(disposition))
    [self removeSelf];
}

@end

InfoBar* AlternateNavInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  AlternateNavInfoBarController* controller =
      [[AlternateNavInfoBarController alloc] initWithDelegate:this owner:owner];
  return new InfoBar(controller, this);
}
