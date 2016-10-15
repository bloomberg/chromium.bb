// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
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
    InfoBarService* infobar_service,
    const GURL& requesting_origin,
    const std::vector<ContentSettingsType>& types) {
  return infobar_service->AddInfoBar(
      base::MakeUnique<GroupedPermissionInfoBar>(base::WrapUnique(
          new GroupedPermissionInfoBarDelegate(requesting_origin, types))));
}

bool GroupedPermissionInfoBarDelegate::ShouldShowPersistenceToggle() const {
  return PermissionUtil::ShouldShowPersistenceToggle();
}

ContentSettingsType GroupedPermissionInfoBarDelegate::GetContentSettingType(
    size_t position) const {
  DCHECK_LT(position, types_.size());
  return types_[position];
}

int GroupedPermissionInfoBarDelegate::GetIconIdForPermission(
    size_t position) const {
  DCHECK_LT(position, types_.size());
  ContentSettingsType type = types_[position];
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    return IDR_INFOBAR_MEDIA_STREAM_MIC;
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
    return IDR_INFOBAR_MEDIA_STREAM_CAMERA;

  return IDR_ANDROID_INFOBAR_WARNING;
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageTextFragment(
    size_t position) const {
  DCHECK_LT(position, types_.size());
  ContentSettingsType type = types_[position];

  const bool is_mic = (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  DCHECK(is_mic || (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  return l10n_util::GetStringUTF16(
      is_mic ? IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT
             : IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT);
}

void GroupedPermissionInfoBarDelegate::ToggleAccept(size_t position,
                                                    bool new_value) {
  DCHECK_LT(position, types_.size());
  accept_states_[position] = new_value;
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_PERMISSIONS_BUBBLE_PROMPT,
      url_formatter::FormatUrlForSecurityDisplay(requesting_origin_));
}

bool GroupedPermissionInfoBarDelegate::GetAcceptState(size_t position) {
  DCHECK_LT(position, types_.size());
  return accept_states_[position];
}

GroupedPermissionInfoBarDelegate::GroupedPermissionInfoBarDelegate(
    const GURL& requesting_origin,
    const std::vector<ContentSettingsType>& types)
    : requesting_origin_(requesting_origin),
      types_(types),
      accept_states_(types_.size(), true),
      persist_(true) {}

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
