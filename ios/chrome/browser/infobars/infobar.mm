// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar.h"

#include <utility>

#include "base/logging.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#include "ios/chrome/browser/translate/translate_infobar_tags.h"
#import "ios/chrome/browser/ui/infobars/infobar_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;

InfoBarIOS::InfoBarIOS(std::unique_ptr<InfoBarDelegate> delegate)
    : InfoBar(std::move(delegate)) {}

InfoBarIOS::~InfoBarIOS() {
  DCHECK(controller_);
  [controller_ detachView];
  controller_.reset();
}

void InfoBarIOS::SetController(InfoBarController* controller) {
  controller_.reset(controller);
}

void InfoBarIOS::Layout(CGRect container_bounds) {
  DCHECK(controller_);
  if ([controller_ view]) {
    [[controller_ view] setFrame:container_bounds];
  } else {
    [controller_ layoutForDelegate:delegate() frame:container_bounds];
  }
  SetBarTargetHeight([controller_ barHeight]);
}

UIView* InfoBarIOS::view() {
  DCHECK(controller_);
  return [controller_ view];
}

void InfoBarIOS::RemoveView() {
  DCHECK(controller_);
  [controller_ removeView];
}

void InfoBarIOS::PlatformSpecificOnHeightsRecalculated() {
  DCHECK(controller_);
  [controller_ onHeightsRecalculated:bar_height()];
}

#pragma mark - InfoBarViewDelegate

void InfoBarIOS::SetInfoBarTargetHeight(int height) {
  SetBarTargetHeight(height);
}

// Some infobar button was pressed.
void InfoBarIOS::InfoBarButtonDidPress(NSUInteger button_id) {
  // Do not add new logic for specific info bar delegates.
  // TODO(droger): Move the logic elsewhere, http://crbug.com/307552.
  // If not owned, the infobar has already been removed.
  if (!owner())
    return;
  if (delegate()->AsConfirmInfoBarDelegate()) {
    ConfirmInfoBarDelegate* confirmDelegate =
        delegate()->AsConfirmInfoBarDelegate();
    if ((button_id == ConfirmInfoBarDelegate::BUTTON_OK &&
         confirmDelegate->Accept()) ||
        (button_id == ConfirmInfoBarDelegate::BUTTON_CANCEL &&
         delegate()->AsConfirmInfoBarDelegate()->Cancel())) {
      RemoveSelf();
    }
  } else if (delegate()->AsTranslateInfoBarDelegate()) {
    translate::TranslateInfoBarDelegate* translateDelegate =
        delegate()->AsTranslateInfoBarDelegate();
    switch (button_id) {
      case TranslateInfoBarIOSTag::AFTER_DONE:
        InfoBarDidCancel();
        break;
      case TranslateInfoBarIOSTag::AFTER_REVERT:
        translateDelegate->RevertTranslation();
        break;
      case TranslateInfoBarIOSTag::BEFORE_ACCEPT:
        translateDelegate->Translate();
        break;
      case TranslateInfoBarIOSTag::BEFORE_DENY:
        translateDelegate->TranslationDeclined();
        if (translateDelegate->ShouldShowNeverTranslateShortcut())
          translateDelegate->ShowNeverTranslateInfobar();
        else
          RemoveSelf();
        break;
      case TranslateInfoBarIOSTag::DENY_LANGUAGE:
        translateDelegate->NeverTranslatePageLanguage();
        RemoveSelf();
        break;
      case TranslateInfoBarIOSTag::DENY_WEBSITE:
        if (!translateDelegate->IsSiteBlacklisted())
          translateDelegate->ToggleSiteBlacklist();
        RemoveSelf();
        break;
      case TranslateInfoBarIOSTag::MESSAGE:
        translateDelegate->MessageInfoBarButtonPressed();
        break;
      default:
        NOTREACHED() << "Unexpected Translate button label";
        break;
    }
  }
}

void InfoBarIOS::InfoBarDidCancel() {
  // If not owned, the infobar has already been removed.
  if (!owner())
    return;
  delegate()->InfoBarDismissed();
  RemoveSelf();
}
