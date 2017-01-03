// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_message_infobar_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/infobar_view.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_delegate.h"
#include "ui/gfx/image/image.h"

@interface TranslateMessageInfoBarController ()

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;

@end

@implementation TranslateMessageInfoBarController

- (InfoBarView*)viewForDelegate:(infobars::InfoBarDelegate*)delegate
                          frame:(CGRect)frame {
  base::scoped_nsobject<InfoBarView> infoBarView;
  translate::TranslateInfoBarDelegate* translateInfoBarDelegate =
      delegate->AsTranslateInfoBarDelegate();
  infoBarView.reset(
      [[InfoBarView alloc] initWithFrame:frame delegate:self.delegate]);
  // Icon
  gfx::Image icon = translateInfoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView addLeftIcon:icon.ToUIImage()];
  // Text.
  [infoBarView addLabel:base::SysUTF16ToNSString(
                            translateInfoBarDelegate->GetMessageInfoBarText())];
  // Close button.
  [infoBarView addCloseButtonWithTag:TranslateInfoBarIOSTag::CLOSE
                              target:self
                              action:@selector(infoBarButtonDidPress:)];
  // Other button.
  base::string16 buttonText(
      translateInfoBarDelegate->GetMessageInfoBarButtonText());
  if (!buttonText.empty()) {
    [infoBarView addButton:base::SysUTF16ToNSString(buttonText)
                       tag:TranslateInfoBarIOSTag::MESSAGE
                    target:self
                    action:@selector(infoBarButtonDidPress:)];
  }
  return [[infoBarView retain] autorelease];
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
    NSUInteger buttonId = static_cast<UIButton*>(sender).tag;
    if (buttonId == TranslateInfoBarIOSTag::CLOSE) {
      self.delegate->InfoBarDidCancel();
    } else {
      DCHECK(buttonId == TranslateInfoBarIOSTag::MESSAGE);
      self.delegate->InfoBarButtonDidPress(buttonId);
    }
  }
}

@end
