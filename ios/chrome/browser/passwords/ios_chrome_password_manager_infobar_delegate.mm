// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

IOSChromePasswordManagerInfoBarDelegate::
    ~IOSChromePasswordManagerInfoBarDelegate() {
  password_manager::metrics_util::LogUIDismissalReason(infobar_response_);
};

IOSChromePasswordManagerInfoBarDelegate::
    IOSChromePasswordManagerInfoBarDelegate(
        bool is_smart_lock_branding_enabled,
        std::unique_ptr<password_manager::PasswordFormManager> form_to_save)
    : form_to_save_(std::move(form_to_save)),
      infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION),
      is_smart_lock_branding_enabled_(is_smart_lock_branding_enabled) {}

infobars::InfoBarDelegate::Type
IOSChromePasswordManagerInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
};

base::string16 IOSChromePasswordManagerInfoBarDelegate::GetLinkText() const {
  return is_smart_lock_branding_enabled_
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK)
             : base::string16();
};

int IOSChromePasswordManagerInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_SAVE_PASSWORD;
};

bool IOSChromePasswordManagerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  base::scoped_nsobject<OpenUrlCommand> command([[OpenUrlCommand alloc]
       initWithURL:GURL(password_manager::kPasswordManagerHelpCenterSmartLock)
          referrer:web::Referrer()
       inIncognito:NO
      inBackground:NO
          appendTo:kCurrentTab]);
  UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
  [mainWindow chromeExecuteCommand:command];
  return true;
};
