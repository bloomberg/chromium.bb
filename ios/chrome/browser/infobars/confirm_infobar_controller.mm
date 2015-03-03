// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_delegate.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/gfx/image/image.h"

namespace {

// UI Tags for the infobar buttons
enum ConfirmInfoBarUITags { OK = 1, CANCEL, CLOSE };

// Converts a UI button tag to the corresponding InfoBarButton.
ConfirmInfoBarDelegate::InfoBarButton UITagToButton(NSUInteger tag) {
  switch (tag) {
    case ConfirmInfoBarUITags::OK:
      return ConfirmInfoBarDelegate::BUTTON_OK;
    case ConfirmInfoBarUITags::CANCEL:
    case ConfirmInfoBarUITags::CLOSE:
      return ConfirmInfoBarDelegate::BUTTON_CANCEL;
    default:
      NOTREACHED();
      return ConfirmInfoBarDelegate::BUTTON_CANCEL;
  }
}

}  // namespace

#pragma mark - ConfirmInfoBarController

@interface ConfirmInfoBarController ()

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;

@end

@implementation ConfirmInfoBarController {
}

#pragma mark -
#pragma mark InfoBarController

- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)frame {
  ConfirmInfoBarDelegate* infoBarModel = delegate->AsConfirmInfoBarDelegate();
  DCHECK(!infoBarView_);
  infoBarView_.reset([ios::GetChromeBrowserProvider()->CreateInfoBarView()
      initWithFrame:frame
           delegate:delegate_]);

  // Model data.
  NSString* modelMsg = nil;
  if (infoBarModel->GetMessageText().length())
    modelMsg = base::SysUTF16ToNSString(infoBarModel->GetMessageText());
  gfx::Image modelIcon = infoBarModel->GetIcon();
  int buttons = infoBarModel->GetButtons();
  NSString* buttonOK = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    buttonOK = base::SysUTF16ToNSString(
        infoBarModel->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  }
  NSString* buttonCancel = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    buttonCancel = base::SysUTF16ToNSString(
        infoBarModel->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  }

  [infoBarView_ addCloseButtonWithTag:ConfirmInfoBarUITags::CLOSE
                               target:self
                               action:@selector(infoBarButtonDidPress:)];

  // Optional left icon.
  if (!modelIcon.IsEmpty())
    [infoBarView_ addLeftIcon:modelIcon.ToUIImage()];

  // Optional message.
  if (modelMsg)
    [infoBarView_ addLabel:modelMsg];

  if (buttonOK && buttonCancel) {
    [infoBarView_ addButton1:buttonOK
                        tag1:ConfirmInfoBarUITags::OK
                     button2:buttonCancel
                        tag2:ConfirmInfoBarUITags::CANCEL
                      target:self
                      action:@selector(infoBarButtonDidPress:)];
  } else if (buttonOK) {
    [infoBarView_ addButton:buttonOK
                        tag:ConfirmInfoBarUITags::OK
                     target:self
                     action:@selector(infoBarButtonDidPress:)];
  } else {
    // No buttons, only message.
    DCHECK(modelMsg && !buttonCancel);
  }
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  // This press might have occurred after the user has already pressed a button,
  // in which case the view has been detached from the delegate and this press
  // should be ignored.
  if (!delegate_) {
    return;
  }
  if ([sender isKindOfClass:[UIButton class]]) {
    NSUInteger tag = static_cast<UIButton*>(sender).tag;
    if (tag == ConfirmInfoBarUITags::CLOSE)
      delegate_->InfoBarDidCancel();
    else
      delegate_->InfoBarButtonDidPress(UITagToButton(tag));
  }
}

@end
