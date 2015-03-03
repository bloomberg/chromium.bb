// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/never_translate_infobar_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "grit/components_strings.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/string_provider.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_delegate.h"
#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

@interface NeverTranslateInfoBarController ()

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;

@end

@implementation NeverTranslateInfoBarController

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)frame {
  translate::TranslateInfoBarDelegate* translateInfoBarDelegate =
      delegate->AsTranslateInfoBarDelegate();
  DCHECK(!infoBarView_);
  ios::ChromeBrowserProvider* provider = ios::GetChromeBrowserProvider();
  infoBarView_.reset([provider->CreateInfoBarView()
      initWithFrame:frame
           delegate:delegate_]);
  // Icon
  gfx::Image icon = translateInfoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView_ addLeftIcon:icon.ToUIImage()];
  // Main text.
  base::string16 originalLanguage = translateInfoBarDelegate->language_name_at(
      translateInfoBarDelegate->original_language_index());
  [infoBarView_ addLabel:l10n_util::GetNSStringF(
                             IDS_TRANSLATE_INFOBAR_NEVER_MESSAGE_IOS,
                             provider->GetStringProvider()->GetProductName(),
                             originalLanguage)];
  // Close button.
  [infoBarView_ addCloseButtonWithTag:TranslateInfoBarIOSTag::CLOSE
                               target:self
                               action:@selector(infoBarButtonDidPress:)];
  // Other buttons.
  NSString* buttonLanguage = l10n_util::GetNSStringF(
      IDS_TRANSLATE_INFOBAR_NEVER_TRANSLATE, originalLanguage);
  NSString* buttonSite = l10n_util::GetNSString(
      IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE);
  [infoBarView_ addButton1:buttonLanguage
                      tag1:TranslateInfoBarIOSTag::DENY_LANGUAGE
                   button2:buttonSite
                      tag2:TranslateInfoBarIOSTag::DENY_WEBSITE
                    target:self
                    action:@selector(infoBarButtonDidPress:)];
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
      DCHECK(buttonId == TranslateInfoBarIOSTag::DENY_LANGUAGE ||
             buttonId == TranslateInfoBarIOSTag::DENY_WEBSITE);
      delegate_->InfoBarButtonDidPress(buttonId);
    }
  }
}

@end
