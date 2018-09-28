// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

#include <utility>

#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromePasswordManagerInfoBarDelegate::
    ~IOSChromePasswordManagerInfoBarDelegate() = default;

IOSChromePasswordManagerInfoBarDelegate::
    IOSChromePasswordManagerInfoBarDelegate(
        bool is_sync_user,
        std::unique_ptr<password_manager::PasswordFormManagerForUI>
            form_to_save)
    : form_to_save_(std::move(form_to_save)),
      infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION),
      is_sync_user_(is_sync_user) {}

int IOSChromePasswordManagerInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_SAVE_PASSWORD;
};
