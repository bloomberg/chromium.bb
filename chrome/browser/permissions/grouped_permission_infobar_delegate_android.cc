// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_prompt_android.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/android/infobars/grouped_permission_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

GroupedPermissionInfoBarDelegate::~GroupedPermissionInfoBarDelegate() {}

// static
infobars::InfoBar* GroupedPermissionInfoBarDelegate::Create(
    PermissionPromptAndroid* permission_prompt,
    InfoBarService* infobar_service,
    const GURL& requesting_origin,
    const std::vector<PermissionRequest*>& requests) {
  return infobar_service->AddInfoBar(base::MakeUnique<GroupedPermissionInfoBar>(
      base::WrapUnique(new GroupedPermissionInfoBarDelegate(
          permission_prompt, requesting_origin, requests))));
}

bool GroupedPermissionInfoBarDelegate::ShouldShowPersistenceToggle() const {
  return PermissionUtil::ShouldShowPersistenceToggle();
}

ContentSettingsType GroupedPermissionInfoBarDelegate::GetContentSettingType(
    size_t position) const {
  DCHECK_LT(position, requests_.size());
  return requests_[position]->GetContentSettingsType();
}

int GroupedPermissionInfoBarDelegate::GetIconIdForPermission(
    size_t position) const {
  DCHECK_LT(position, requests_.size());
  return requests_[position]->GetIconId();
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageTextFragment(
    size_t position) const {
  DCHECK_LT(position, requests_.size());
  return requests_[position]->GetMessageTextFragment();
}

void GroupedPermissionInfoBarDelegate::ToggleAccept(size_t position,
                                                    bool new_value) {
  DCHECK_LT(position, requests_.size());
  if (permission_prompt_)
    permission_prompt_->ToggleAccept(position, new_value);
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_PERMISSIONS_BUBBLE_PROMPT,
      url_formatter::FormatUrlForSecurityDisplay(requesting_origin_));
}

bool GroupedPermissionInfoBarDelegate::Accept() {
  if (permission_prompt_)
    permission_prompt_->Accept();
  return true;
}

bool GroupedPermissionInfoBarDelegate::Cancel() {
  if (permission_prompt_)
    permission_prompt_->Deny();
  return true;
}

void GroupedPermissionInfoBarDelegate::InfoBarDismissed() {
  if (permission_prompt_)
    permission_prompt_->Closing();
}

void GroupedPermissionInfoBarDelegate::PermissionPromptDestroyed() {
  permission_prompt_ = nullptr;
}

GroupedPermissionInfoBarDelegate::GroupedPermissionInfoBarDelegate(
    PermissionPromptAndroid* permission_prompt,
    const GURL& requesting_origin,
    const std::vector<PermissionRequest*>& requests)
    : requesting_origin_(requesting_origin),
      requests_(requests),
      persist_(true),
      permission_prompt_(permission_prompt) {
  DCHECK(permission_prompt);
}

infobars::InfoBarDelegate::InfoBarIdentifier
GroupedPermissionInfoBarDelegate::GetIdentifier() const {
  return GROUPED_PERMISSION_INFOBAR_DELEGATE_ANDROID;
}

infobars::InfoBarDelegate::Type
GroupedPermissionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int GroupedPermissionInfoBarDelegate::GetButtons() const {
  // If there is only one permission in the infobar, we show both OK and CANCEL
  // button to allow/deny it. If there are multiple, we only show OK button
  // which means making decision for all permissions according to each accept
  // toggle.
  return (permission_count() > 1) ? BUTTON_OK : (BUTTON_OK | BUTTON_CANCEL);
}

base::string16 GroupedPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (permission_count() > 1) {
    return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_APP_OK
                                                           : IDS_APP_CANCEL);
  }

  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_PERMISSION_ALLOW
                                                         : IDS_PERMISSION_DENY);
}
