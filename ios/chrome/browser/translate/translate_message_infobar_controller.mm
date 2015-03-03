// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_message_infobar_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_delegate.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/gfx/image/image.h"

@interface TranslateMessageInfoBarController ()

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;

@end

@implementation TranslateMessageInfoBarController

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)frame {
  translate::TranslateInfoBarDelegate* translateInfoBarDelegate =
      delegate->AsTranslateInfoBarDelegate();
  DCHECK(!infoBarView_);
  infoBarView_.reset([ios::GetChromeBrowserProvider()->CreateInfoBarView()
      initWithFrame:frame
           delegate:delegate_]);
  // Icon
  gfx::Image icon = translateInfoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView_ addLeftIcon:icon.ToUIImage()];
  // Text.
  [infoBarView_
      addLabel:base::SysUTF16ToNSString(
                   translateInfoBarDelegate->GetMessageInfoBarText())];
  // Close button.
  [infoBarView_ addCloseButtonWithTag:TranslateInfoBarIOSTag::CLOSE
                               target:self
                               action:@selector(infoBarButtonDidPress:)];
  // Other button.
  base::string16 buttonText(
      translateInfoBarDelegate->GetMessageInfoBarButtonText());
  if (!buttonText.empty()) {
    [infoBarView_ addButton:base::SysUTF16ToNSString(buttonText)
                        tag:TranslateInfoBarIOSTag::MESSAGE
                     target:self
                     action:@selector(infoBarButtonDidPress:)];
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
    NSUInteger buttonId = static_cast<UIButton*>(sender).tag;
    if (buttonId == TranslateInfoBarIOSTag::CLOSE) {
      delegate_->InfoBarDidCancel();
    } else {
      DCHECK(buttonId == TranslateInfoBarIOSTag::MESSAGE);
      delegate_->InfoBarButtonDidPress(buttonId);
    }
  }
}

@end
