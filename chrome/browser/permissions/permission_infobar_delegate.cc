// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_infobar_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

PermissionInfobarDelegate::~PermissionInfobarDelegate() {
  if (!action_taken_) {
    PermissionDecisionAutoBlocker(profile_).RecordIgnore(requesting_origin_,
                                                         permission_type_);

    PermissionUmaUtil::PermissionIgnored(
        permission_type_,
        user_gesture_ ? PermissionRequestGestureType::GESTURE
                      : PermissionRequestGestureType::NO_GESTURE,
        requesting_origin_, profile_);
  }
}

PermissionInfobarDelegate::PermissionInfobarDelegate(
    const GURL& requesting_origin,
    content::PermissionType permission_type,
    ContentSettingsType content_settings_type,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback)
    : requesting_origin_(requesting_origin),
      permission_type_(permission_type),
      content_settings_type_(content_settings_type),
      profile_(profile),
      callback_(callback),
      action_taken_(false),
      user_gesture_(user_gesture),
      persist_(true) {}

bool PermissionInfobarDelegate::ShouldShowPersistenceToggle() const {
  // Only show the persistence toggle for geolocation.
  if (permission_type_ == content::PermissionType::GEOLOCATION) {
    return base::FeatureList::IsEnabled(
        features::kDisplayPersistenceToggleInPermissionPrompts);
  }
  return false;
}

base::string16 PermissionInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      GetMessageResourceId(),
      url_formatter::FormatUrlForSecurityDisplay(
          requesting_origin_,
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

infobars::InfoBarDelegate::Type PermissionInfobarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

void PermissionInfobarDelegate::InfoBarDismissed() {
  SetPermission(false, DISMISSED);
}

PermissionInfobarDelegate*
PermissionInfobarDelegate::AsPermissionInfobarDelegate() {
  return this;
}

base::string16 PermissionInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PERMISSION_ALLOW : IDS_PERMISSION_DENY);
}

bool PermissionInfobarDelegate::Accept() {
  bool update_content_setting = true;
  if (ShouldShowPersistenceToggle()) {
    update_content_setting = persist_;
    PermissionUmaUtil::PermissionPromptAcceptedWithPersistenceToggle(
        permission_type_, persist_);
  }

  SetPermission(update_content_setting, GRANTED);
  return true;
}

bool PermissionInfobarDelegate::Cancel() {
  bool update_content_setting = true;
  if (ShouldShowPersistenceToggle()) {
    update_content_setting = persist_;
    PermissionUmaUtil::PermissionPromptDeniedWithPersistenceToggle(
        permission_type_, persist_);
  }

  SetPermission(update_content_setting, DENIED);
  return true;
}

void PermissionInfobarDelegate::SetPermission(bool update_content_setting,
                                              PermissionAction decision) {
  action_taken_ = true;
  callback_.Run(update_content_setting, decision);
}
