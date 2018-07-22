// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/supervised_users_deprecated_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/chromium_strings.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kSupervisedUsersDeprecatedLearnMoreLink[] =
    "https://support.google.com.com/chrome?p=supervised_users";
}  // namespace

// static
void SupervisedUsersDeprecatedInfoBarDelegate::Create(
    InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      base::WrapUnique(static_cast<ConfirmInfoBarDelegate*>(
          new SupervisedUsersDeprecatedInfoBarDelegate()))));
}

SupervisedUsersDeprecatedInfoBarDelegate::
    SupervisedUsersDeprecatedInfoBarDelegate()
    : ConfirmInfoBarDelegate() {}

SupervisedUsersDeprecatedInfoBarDelegate::
    ~SupervisedUsersDeprecatedInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
SupervisedUsersDeprecatedInfoBarDelegate::GetIdentifier() const {
  return SUPERVISED_USERS_DEPRECATED_INFOBAR_DELEGATE;
}

base::string16 SupervisedUsersDeprecatedInfoBarDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(IDS_SUPERVISED_USERS_DEPRECATED_MESSAGE);
}

int SupervisedUsersDeprecatedInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 SupervisedUsersDeprecatedInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL SupervisedUsersDeprecatedInfoBarDelegate::GetLinkURL() const {
  return GURL(kSupervisedUsersDeprecatedLearnMoreLink);
}

bool SupervisedUsersDeprecatedInfoBarDelegate::ShouldExpire(
    const NavigationDetails& navigation_details) const {
  return false;
}
