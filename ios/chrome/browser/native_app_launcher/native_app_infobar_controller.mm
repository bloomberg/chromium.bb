// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/native_app_launcher/native_app_infobar_controller.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"
#include "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/base/l10n/l10n_util.h"

@interface NativeAppInfoBarController ()

// Action for any of the user defined buttons.
- (void)infoBarButtonDidPress:(UIButton*)button;

// Notifies the delegate of which action the user took.
- (void)userPerformedAction:(NativeAppActionType)userAction;

// For testing.
// Sets the infobar delegate.
- (void)setInfoBarDelegate:(NativeAppInfoBarDelegate*)delegate;

@end

@implementation NativeAppInfoBarController {
  NativeAppInfoBarDelegate* nativeAppInfoBarDelegate_;  // weak
}

#pragma mark - InfoBarController

- (base::scoped_nsobject<UIView<InfoBarViewProtocol>>)
viewForDelegate:(infobars::InfoBarDelegate*)delegate
          frame:(CGRect)frame {
  base::scoped_nsobject<UIView<InfoBarViewProtocol>> infoBarView;
  nativeAppInfoBarDelegate_ = static_cast<NativeAppInfoBarDelegate*>(delegate);
  DCHECK(nativeAppInfoBarDelegate_);
  infoBarView.reset(
      ios::GetChromeBrowserProvider()->CreateInfoBarView(frame, self.delegate));

  // Lays out widgets common to all NativeAppInfobars.
  [infoBarView
      addPlaceholderTransparentIcon:native_app_infobar::kSmallIconSize];
  nativeAppInfoBarDelegate_->FetchSmallAppIcon(^(UIImage* image) {
    [infoBarView addLeftIconWithRoundedCornersAndShadow:image];
  });
  [infoBarView addCloseButtonWithTag:NATIVE_APP_ACTION_DISMISS
                              target:self
                              action:@selector(infoBarButtonDidPress:)];

  auto type = nativeAppInfoBarDelegate_->GetControllerType();
  switch (type) {
    case NATIVE_APP_INSTALLER_CONTROLLER: {
      NSString* buttonMsg =
          l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_INSTALL_APP_BUTTON_LABEL);
      [infoBarView addButton:buttonMsg
                         tag:NATIVE_APP_ACTION_CLICK_INSTALL
                      target:self
                      action:@selector(infoBarButtonDidPress:)];
      DCHECK(nativeAppInfoBarDelegate_->GetInstallText().length());
      [infoBarView addLabel:base::SysUTF16ToNSString(
                                nativeAppInfoBarDelegate_->GetInstallText())];
      break;
    }
    case NATIVE_APP_LAUNCHER_CONTROLLER: {
      NSString* buttonMsg =
          l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
      [infoBarView addButton:buttonMsg
                         tag:NATIVE_APP_ACTION_CLICK_LAUNCH
                      target:self
                      action:@selector(infoBarButtonDidPress:)];
      DCHECK(nativeAppInfoBarDelegate_->GetLaunchText().length());
      [infoBarView addLabel:base::SysUTF16ToNSString(
                                nativeAppInfoBarDelegate_->GetLaunchText())];
      break;
    }
    case NATIVE_APP_OPEN_POLICY_CONTROLLER: {
      NSString* labelMsg = base::SysUTF16ToNSString(
          nativeAppInfoBarDelegate_->GetOpenPolicyText());
      DCHECK(nativeAppInfoBarDelegate_->GetOpenPolicyText().length());
      NSString* buttonOnceString = base::SysUTF16ToNSString(
          nativeAppInfoBarDelegate_->GetOpenOnceText());
      NSString* buttonAlwaysString = base::SysUTF16ToNSString(
          nativeAppInfoBarDelegate_->GetOpenAlwaysText());
      [infoBarView addLabel:labelMsg];
      [infoBarView addButton1:buttonOnceString
                         tag1:NATIVE_APP_ACTION_CLICK_ONCE
                      button2:buttonAlwaysString
                         tag2:NATIVE_APP_ACTION_CLICK_ALWAYS
                       target:self
                       action:@selector(infoBarButtonDidPress:)];
      break;
    }
  }
  return infoBarView;
}

- (void)infoBarButtonDidPress:(UIButton*)button {
  DCHECK(nativeAppInfoBarDelegate_);
  DCHECK([button isKindOfClass:[UIButton class]]);
  // This press might have occurred after the user has already pressed a button,
  // in which case the view has been detached from the delegate and this press
  // should be ignored.
  if (!self.delegate) {
    return;
  }
  self.delegate->InfoBarDidCancel();
  NativeAppActionType action = static_cast<NativeAppActionType>([button tag]);
  [self userPerformedAction:action];
}

- (void)userPerformedAction:(NativeAppActionType)userAction {
  nativeAppInfoBarDelegate_->UserPerformedAction(userAction);
}

#pragma mark - Testing

- (void)setInfoBarDelegate:(NativeAppInfoBarDelegate*)delegate {
  nativeAppInfoBarDelegate_ = delegate;
}

@end
