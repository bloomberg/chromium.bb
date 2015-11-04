// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_delegate.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"

namespace {

// UI Tags for the infobar elements.
enum ConfirmInfoBarUITags { OK = 1, CANCEL, CLOSE, TITLE_LINK };

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
// Action for any of the user defined links.
- (void)infobarLinkDidPress:(NSNumber*)tag;
- (void)updateInfobarLabel:(UIView<InfoBarViewProtocol>*)view;
@end

@implementation ConfirmInfoBarController {
  ConfirmInfoBarDelegate* confirmInfobarDelegate_;  // weak
}

#pragma mark -
#pragma mark InfoBarController

- (base::scoped_nsobject<UIView<InfoBarViewProtocol>>)
    viewForDelegate:(infobars::InfoBarDelegate*)delegate
              frame:(CGRect)frame {
  base::scoped_nsobject<UIView<InfoBarViewProtocol>> infoBarView;
  confirmInfobarDelegate_ = delegate->AsConfirmInfoBarDelegate();
  infoBarView.reset(
      ios::GetChromeBrowserProvider()->CreateInfoBarView(frame, self.delegate));
  // Model data.
  gfx::Image modelIcon = confirmInfobarDelegate_->GetIcon();
  int buttons = confirmInfobarDelegate_->GetButtons();
  NSString* buttonOK = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    buttonOK = base::SysUTF16ToNSString(confirmInfobarDelegate_->GetButtonLabel(
        ConfirmInfoBarDelegate::BUTTON_OK));
  }
  NSString* buttonCancel = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    buttonCancel =
        base::SysUTF16ToNSString(confirmInfobarDelegate_->GetButtonLabel(
            ConfirmInfoBarDelegate::BUTTON_CANCEL));
  }

  [infoBarView addCloseButtonWithTag:ConfirmInfoBarUITags::CLOSE
                              target:self
                              action:@selector(infoBarButtonDidPress:)];

  // Optional left icon.
  if (!modelIcon.IsEmpty())
    [infoBarView addLeftIcon:modelIcon.ToUIImage()];

  // Optional message.
  [self updateInfobarLabel:infoBarView];

  if (buttonOK && buttonCancel) {
    [infoBarView addButton1:buttonOK
                       tag1:ConfirmInfoBarUITags::OK
                    button2:buttonCancel
                       tag2:ConfirmInfoBarUITags::CANCEL
                     target:self
                     action:@selector(infoBarButtonDidPress:)];
  } else if (buttonOK) {
    [infoBarView addButton:buttonOK
                       tag:ConfirmInfoBarUITags::OK
                    target:self
                    action:@selector(infoBarButtonDidPress:)];
  } else {
    // No buttons, only message.
    DCHECK(!confirmInfobarDelegate_->GetMessageText().empty() && !buttonCancel);
  }
  return infoBarView;
}

- (void)updateInfobarLabel:(UIView<InfoBarViewProtocol>*)view {
  if (!confirmInfobarDelegate_->GetMessageText().length())
    return;
  if (confirmInfobarDelegate_->GetLinkText().length()) {
    base::string16 msgLink = base::SysNSStringToUTF16(
        [[view class] stringAsLink:base::SysUTF16ToNSString(
                                       confirmInfobarDelegate_->GetLinkText())
                               tag:ConfirmInfoBarUITags::TITLE_LINK]);
    base::string16 messageText = confirmInfobarDelegate_->GetMessageText();
    base::ReplaceFirstSubstringAfterOffset(
        &messageText, 0, confirmInfobarDelegate_->GetLinkText(), msgLink);

    [view addLabel:base::SysUTF16ToNSString(messageText)
            target:self
            action:@selector(infobarLinkDidPress:)];
  } else {
    NSString* label =
        base::SysUTF16ToNSString(confirmInfobarDelegate_->GetMessageText());
    [view addLabel:label];
  }
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  // This press might have occurred after the user has already pressed a button,
  // in which case the view has been detached from the delegate and this press
  // should be ignored.
  if (!self.delegate) {
    return;
  }
  if ([sender isKindOfClass:[UIButton class]]) {
    NSUInteger tag = static_cast<UIButton*>(sender).tag;
    if (tag == ConfirmInfoBarUITags::CLOSE)
      self.delegate->InfoBarDidCancel();
    else
      self.delegate->InfoBarButtonDidPress(UITagToButton(tag));
  }
}

// Title link was clicked.
- (void)infobarLinkDidPress:(NSNumber*)tag {
  DCHECK([tag isKindOfClass:[NSNumber class]]);
  if (!self.delegate) {
    return;
  }
  if ([tag unsignedIntegerValue] == ConfirmInfoBarUITags::TITLE_LINK) {
    confirmInfobarDelegate_->LinkClicked(NEW_FOREGROUND_TAB);
  }
}

@end
