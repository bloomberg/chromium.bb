// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/native_app_launcher/native_app_infobar_controller.h"
#import "ios/chrome/browser/native_app_launcher/native_app_navigation_controller_protocol.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace native_app_infobar {
const CGSize kSmallIconSize = {29.0, 29.0};
}  // namespace native_app_infobar

NativeAppInfoBarDelegate::NativeAppInfoBarDelegate(
    id<NativeAppNavigationControllerProtocol> controller,
    const GURL& page_url,
    NativeAppControllerType type)
    : controller_(controller), page_url_(page_url), type_(type) {}

NativeAppInfoBarDelegate::~NativeAppInfoBarDelegate() {}

// static
bool NativeAppInfoBarDelegate::Create(
    infobars::InfoBarManager* manager,
    id<NativeAppNavigationControllerProtocol> controller_protocol,
    const GURL& page_url,
    NativeAppControllerType type) {
  DCHECK(manager);
  auto infobar =
      base::MakeUnique<InfoBarIOS>(base::MakeUnique<NativeAppInfoBarDelegate>(
          controller_protocol, page_url, type));
  base::scoped_nsobject<NativeAppInfoBarController> controller(
      [[NativeAppInfoBarController alloc] initWithDelegate:infobar.get()]);
  infobar->SetController(controller);
  return !!manager->AddInfoBar(std::move(infobar));
}

bool NativeAppInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return details.is_navigation_to_different_page;
}

NativeAppInfoBarDelegate*
NativeAppInfoBarDelegate::AsNativeAppInfoBarDelegate() {
  return this;
}

base::string16 NativeAppInfoBarDelegate::GetInstallText() const {
  return l10n_util::GetStringFUTF16(
      IDS_IOS_APP_LAUNCHER_OPEN_IN_APP_QUESTION_MESSAGE,
      base::SysNSStringToUTF16([controller_ appName]));
}

base::string16 NativeAppInfoBarDelegate::GetLaunchText() const {
  return l10n_util::GetStringFUTF16(
      IDS_IOS_APP_LAUNCHER_OPEN_IN_APP_QUESTION_MESSAGE,
      base::SysNSStringToUTF16([controller_ appName]));
}

base::string16 NativeAppInfoBarDelegate::GetOpenPolicyText() const {
  return l10n_util::GetStringFUTF16(
      IDS_IOS_APP_LAUNCHER_OPEN_IN_LABEL,
      base::SysNSStringToUTF16([controller_ appName]));
}

base::string16 NativeAppInfoBarDelegate::GetOpenOnceText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_APP_LAUNCHER_OPEN_ONCE_BUTTON);
}

base::string16 NativeAppInfoBarDelegate::GetOpenAlwaysText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_APP_LAUNCHER_OPEN_ALWAYS_BUTTON);
}

NSString* NativeAppInfoBarDelegate::GetAppId() const {
  return [controller_ appId];
}

infobars::InfoBarDelegate::Type NativeAppInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
NativeAppInfoBarDelegate::GetIdentifier() const {
  switch (type_) {
    case NATIVE_APP_INSTALLER_CONTROLLER:
      return NATIVE_APP_INSTALLER_INFOBAR_DELEGATE;
    case NATIVE_APP_LAUNCHER_CONTROLLER:
      return NATIVE_APP_LAUNCHER_INFOBAR_DELEGATE;
    case NATIVE_APP_OPEN_POLICY_CONTROLLER:
      return NATIVE_APP_OPEN_POLICY_INFOBAR_DELEGATE;
  }
}

bool NativeAppInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  NativeAppInfoBarDelegate* nativeAppInfoBarDelegate =
      delegate->AsNativeAppInfoBarDelegate();
  return nativeAppInfoBarDelegate &&
         [nativeAppInfoBarDelegate->GetAppId() isEqualToString:GetAppId()];
}

void NativeAppInfoBarDelegate::FetchSmallAppIcon(void (^block)(UIImage*)) {
  [controller_ fetchSmallIconWithCompletionBlock:block];
}

void NativeAppInfoBarDelegate::UserPerformedAction(
    NativeAppActionType userAction) {
  [controller_ updateMetadataWithUserAction:userAction];
  switch (userAction) {
    case NATIVE_APP_ACTION_CLICK_INSTALL:
      DCHECK(type_ == NATIVE_APP_INSTALLER_CONTROLLER);
      [controller_ openStore];
      break;
    case NATIVE_APP_ACTION_CLICK_LAUNCH:
      DCHECK(type_ == NATIVE_APP_LAUNCHER_CONTROLLER);
      [controller_ launchApp:page_url_];
      break;
    case NATIVE_APP_ACTION_CLICK_ONCE:
      DCHECK(type_ == NATIVE_APP_OPEN_POLICY_CONTROLLER);
      [controller_ launchApp:page_url_];
      break;
    case NATIVE_APP_ACTION_CLICK_ALWAYS:
      DCHECK(type_ == NATIVE_APP_OPEN_POLICY_CONTROLLER);
      [controller_ launchApp:page_url_];
      break;
    case NATIVE_APP_ACTION_DISMISS:
      break;
    case NATIVE_APP_ACTION_IGNORE:
    case NATIVE_APP_ACTION_COUNT:
      NOTREACHED();
      break;
  }
}

NativeAppControllerType NativeAppInfoBarDelegate::GetControllerType() const {
  return type_;
}
