// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/confirm_infobar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_view.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// UI Tags for the infobar elements.
typedef NS_ENUM(NSInteger, ConfirmInfoBarUITags) {
  OK = 1,
  CANCEL,
  CLOSE,
  TITLE_LINK
};

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

@interface ConfirmInfoBarController () {
  ConfirmInfoBarDelegate* _confirmInfobarDelegate;
}
@end

@implementation ConfirmInfoBarController

#pragma mark -
#pragma mark InfoBarController

- (InfoBarView*)viewForDelegate:(infobars::InfoBarDelegate*)delegate
                          frame:(CGRect)frame {
  _confirmInfobarDelegate = delegate->AsConfirmInfoBarDelegate();
  InfoBarView* infoBarView =
      [[InfoBarView alloc] initWithFrame:frame delegate:self.delegate];
  // Model data.
  gfx::Image modelIcon = _confirmInfobarDelegate->GetIcon();
  int buttons = _confirmInfobarDelegate->GetButtons();
  NSString* buttonOK = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    buttonOK = base::SysUTF16ToNSString(_confirmInfobarDelegate->GetButtonLabel(
        ConfirmInfoBarDelegate::BUTTON_OK));
  }
  NSString* buttonCancel = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    buttonCancel =
        base::SysUTF16ToNSString(_confirmInfobarDelegate->GetButtonLabel(
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
    DCHECK(!_confirmInfobarDelegate->GetMessageText().empty() && !buttonCancel);
  }
  return infoBarView;
}

- (void)updateInfobarLabel:(InfoBarView*)view {
  if (!_confirmInfobarDelegate->GetMessageText().length())
    return;
  if (_confirmInfobarDelegate->GetLinkText().length()) {
    base::string16 msgLink = base::SysNSStringToUTF16([[view class]
        stringAsLink:base::SysUTF16ToNSString(
                         _confirmInfobarDelegate->GetLinkText())
                 tag:ConfirmInfoBarUITags::TITLE_LINK]);
    base::string16 messageText = _confirmInfobarDelegate->GetMessageText();
    base::ReplaceFirstSubstringAfterOffset(
        &messageText, 0, _confirmInfobarDelegate->GetLinkText(), msgLink);

    [view addLabel:base::SysUTF16ToNSString(messageText)
            target:self
            action:@selector(infobarLinkDidPress:)];
  } else {
    NSString* label =
        base::SysUTF16ToNSString(_confirmInfobarDelegate->GetMessageText());
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
    _confirmInfobarDelegate->LinkClicked(
        WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

@end
