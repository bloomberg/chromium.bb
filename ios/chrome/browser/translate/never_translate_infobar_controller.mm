// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/never_translate_infobar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NeverTranslateInfoBarController ()

// Overrides superclass property.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* infoBarDelegate;

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(id)sender;

@end

@implementation NeverTranslateInfoBarController

@dynamic infoBarDelegate;

#pragma mark -
#pragma mark InfoBarControllerProtocol

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  return [super initWithInfoBarDelegate:infoBarDelegate];
}

- (UIView*)infobarView {
  ConfirmInfoBarView* infoBarView =
      [[ConfirmInfoBarView alloc] initWithFrame:CGRectZero];
  // Icon
  gfx::Image icon = self.infoBarDelegate->GetIcon();
  if (!icon.IsEmpty())
    [infoBarView addLeftIcon:icon.ToUIImage()];
  // Main text.
  base::string16 originalLanguage =
      self.infoBarDelegate->original_language_name();
  [infoBarView
      addLabel:l10n_util::GetNSStringF(IDS_IOS_TRANSLATE_INFOBAR_NEVER_MESSAGE,
                                       originalLanguage)];
  // Close button.
  [infoBarView addCloseButtonWithTag:TranslateInfoBarIOSTag::CLOSE
                              target:self
                              action:@selector(infoBarButtonDidPress:)];
  // Other buttons.
  NSString* buttonLanguage = l10n_util::GetNSStringF(
      IDS_TRANSLATE_INFOBAR_NEVER_TRANSLATE, originalLanguage);
  NSString* buttonSite = l10n_util::GetNSString(
      IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE);
  [infoBarView addButton1:buttonLanguage
                     tag1:TranslateInfoBarIOSTag::DENY_LANGUAGE
                  button2:buttonSite
                     tag2:TranslateInfoBarIOSTag::DENY_WEBSITE
                   target:self
                   action:@selector(infoBarButtonDidPress:)];
  return infoBarView;
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  NSUInteger buttonId = base::mac::ObjCCastStrict<UIButton>(sender).tag;
  switch (buttonId) {
    case TranslateInfoBarIOSTag::CLOSE:
      self.infoBarDelegate->InfoBarDismissed();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateInfoBarIOSTag::DENY_LANGUAGE:
      self.infoBarDelegate->NeverTranslatePageLanguage();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateInfoBarIOSTag::DENY_WEBSITE:
      if (!self.infoBarDelegate->IsSiteBlacklisted())
        self.infoBarDelegate->ToggleSiteBlacklist();
      self.delegate->RemoveInfoBar();
      break;
    default:
      NOTREACHED() << "Unexpected Translate button label";
      break;
  }
}

@end
