// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/grouped_permission_infobar_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

GroupedPermissionInfoBarDelegate::GroupedPermissionInfoBarDelegate(
    const GURL& requesting_origin,
    const std::vector<ContentSettingsType>& types)
    : requesting_origin_(requesting_origin),
      types_(types),
      accept_states_(types_.size(), true) {}

GroupedPermissionInfoBarDelegate::~GroupedPermissionInfoBarDelegate() {}

infobars::InfoBarDelegate::Type
GroupedPermissionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int GroupedPermissionInfoBarDelegate::GetButtons() const {
  if (GetPermissionCount() >= 2)
    return ConfirmInfoBarDelegate::InfoBarButton::BUTTON_OK;
  else
    return ConfirmInfoBarDelegate::GetButtons();
}

base::string16 GroupedPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (GetPermissionCount() >= 2) {
    return ConfirmInfoBarDelegate::GetButtonLabel(button);
  } else {
    return l10n_util::GetStringUTF16(
        (button == BUTTON_OK) ? IDS_PERMISSION_ALLOW : IDS_PERMISSION_DENY);
  }
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_PERMISSIONS_BUBBLE_PROMPT,
      url_formatter::FormatUrlForSecurityDisplay(requesting_origin_));
}

int GroupedPermissionInfoBarDelegate::GetPermissionCount() const {
  return types_.size();
}

ContentSettingsType GroupedPermissionInfoBarDelegate::GetContentSettingType(
    int index) const {
  DCHECK(index >= 0 && index < static_cast<int>(types_.size()));
  return types_[index];
}

int GroupedPermissionInfoBarDelegate::GetIconIdForPermission(int index) const {
  DCHECK(index >= 0 && index < static_cast<int>(types_.size()));
  ContentSettingsType type = types_[index];
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    return IDR_INFOBAR_MEDIA_STREAM_MIC;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    return IDR_INFOBAR_MEDIA_STREAM_CAMERA;
  }
  return IDR_INFOBAR_WARNING;
}

base::string16 GroupedPermissionInfoBarDelegate::GetMessageTextFragment(
    int index) const {
  DCHECK(index >= 0 && index < static_cast<int>(types_.size()));
  ContentSettingsType type = types_[index];
  int message_id;
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
  } else {
    NOTREACHED();
    return base::string16();
  }
  return l10n_util::GetStringUTF16(message_id);
}

void GroupedPermissionInfoBarDelegate::ToggleAccept(int position,
                                                    bool new_value) {
  DCHECK(position >= 0 && position < static_cast<int>(types_.size()));
  accept_states_[position] = new_value;
}

bool GroupedPermissionInfoBarDelegate::GetAcceptState(int position) {
  DCHECK(position >= 0 && position < static_cast<int>(types_.size()));
  return accept_states_[position];
}
