// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/confirm_infobar_controller.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#include "chrome/browser/ui/cocoa/infobars/infobar_cocoa.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/window_open_disposition.h"

@implementation ConfirmInfoBarController

// Called when someone clicks on the "OK" button.
- (IBAction)ok:(id)sender {
  if (![self isOwned])
    return;
  if ([self delegate]->AsConfirmInfoBarDelegate()->Accept())
    [self removeSelf];
}

// Called when someone clicks on the "Cancel" button.
- (IBAction)cancel:(id)sender {
  if (![self isOwned])
    return;
  if ([self delegate]->AsConfirmInfoBarDelegate()->Cancel())
    [self removeSelf];
}

// Confirm infobars can have OK and/or cancel buttons, depending on
// the return value of GetButtons().  We create each button if
// required and position them to the left of the close button.
- (void)addAdditionalControls {
  ConfirmInfoBarDelegate* delegate =
      [self delegate]->AsConfirmInfoBarDelegate();
  DCHECK(delegate);
  int visibleButtons = delegate->GetButtons();

  NSRect okButtonFrame = [okButton_ frame];
  NSRect cancelButtonFrame = [cancelButton_ frame];

  DCHECK(NSMaxX(cancelButtonFrame) < NSMinX(okButtonFrame))
      << "Ok button expected to be on the right of the Cancel button in nib";

  CGFloat rightEdge = NSMaxX(okButtonFrame);
  CGFloat spaceBetweenButtons =
      NSMinX(okButtonFrame) - NSMaxX(cancelButtonFrame);
  CGFloat spaceBeforeButtons =
      NSMinX(cancelButtonFrame) - NSMaxX([label_.get() frame]);

  // Update and position the OK button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK) {
    [okButton_ setTitle:base::SysUTF16ToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK))];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:okButton_];
    okButtonFrame = [okButton_ frame];

    // Position the ok button to the left of the Close button.
    okButtonFrame.origin.x = rightEdge - okButtonFrame.size.width;
    [okButton_ setFrame:okButtonFrame];

    // Update the rightEdge
    rightEdge = NSMinX(okButtonFrame);
  } else {
    [okButton_ removeFromSuperview];
    okButton_ = nil;
  }

  // Update and position the Cancel button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    [cancelButton_ setTitle:base::SysUTF16ToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL))];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:cancelButton_];
    cancelButtonFrame = [cancelButton_ frame];

    // If we had a Ok button, leave space between the buttons.
    if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK) {
      rightEdge -= spaceBetweenButtons;
    }

    // Position the Cancel button on our current right edge.
    cancelButtonFrame.origin.x = rightEdge - cancelButtonFrame.size.width;
    [cancelButton_ setFrame:cancelButtonFrame];

    // Update the rightEdge.
    rightEdge = NSMinX(cancelButtonFrame);
  } else {
    [cancelButton_ removeFromSuperview];
    cancelButton_ = nil;
  }

  // If we had either button, leave space before the edge of the textfield.
  if ((visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) ||
      (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK)) {
    rightEdge -= spaceBeforeButtons;
  }

  NSRect frame = [label_.get() frame];
  DCHECK(rightEdge > NSMinX(frame))
      << "Need to make the xib larger to handle buttons with text this long";
  frame.size.width = rightEdge - NSMinX(frame);
  [label_.get() setFrame:frame];

  // Set the text and link.
  NSString* message = base::SysUTF16ToNSString(delegate->GetMessageText());
  base::string16 link = delegate->GetLinkText();
  if (!link.empty()) {
    // Add spacing between the label and the link.
    message = [message stringByAppendingString:@"   "];
  }
  NSFont* font = [NSFont labelFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  HyperlinkTextView* view = (HyperlinkTextView*)label_.get();
  [view setMessageAndLink:message
                 withLink:base::SysUTF16ToNSString(link)
                 atOffset:[message length]
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
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if ([self delegate]->AsConfirmInfoBarDelegate()->LinkClicked(disposition))
    [self removeSelf];
}

@end

// static
scoped_ptr<infobars::InfoBar> ConfirmInfoBarDelegate::CreateInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  scoped_ptr<InfoBarCocoa> infobar(
      new InfoBarCocoa(delegate.PassAs<infobars::InfoBarDelegate>()));
  base::scoped_nsobject<ConfirmInfoBarController> controller(
      [[ConfirmInfoBarController alloc] initWithInfoBar:infobar.get()]);
  infobar->set_controller(controller);
  return infobar.PassAs<infobars::InfoBar>();
}
