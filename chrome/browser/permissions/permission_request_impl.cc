// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_impl.h"

#include "build/build_config.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#else
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/vector_icons/vector_icons.h"
#endif

PermissionRequestImpl::PermissionRequestImpl(
    const GURL& request_origin,
    ContentSettingsType content_settings_type,
    Profile* profile,
    bool has_gesture,
    const PermissionDecidedCallback& permission_decided_callback,
    const base::Closure delete_callback)
    : request_origin_(request_origin),
      content_settings_type_(content_settings_type),
      profile_(profile),
      has_gesture_(has_gesture),
      permission_decided_callback_(permission_decided_callback),
      delete_callback_(delete_callback),
      is_finished_(false),
      action_taken_(false) {}

PermissionRequestImpl::~PermissionRequestImpl() {
  DCHECK(is_finished_);
  if (!action_taken_) {
    PermissionUmaUtil::PermissionIgnored(
        content_settings_type_, GetGestureType(), request_origin_, profile_);
    PermissionUmaUtil::RecordEmbargoStatus(
        PermissionEmbargoStatus::NOT_EMBARGOED);
  }
}

PermissionRequest::IconId PermissionRequestImpl::GetIconId() const {
#if defined(OS_ANDROID)
  switch (content_settings_type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return IDR_ANDROID_INFOBAR_GEOLOCATION;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return IDR_ANDROID_INFOBAR_NOTIFICATIONS;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return IDR_ANDROID_INFOBAR_MIDI;
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
    default:
      NOTREACHED();
      return IDR_ANDROID_INFOBAR_WARNING;
  }
#else
  switch (content_settings_type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return ui::kLocationOnIcon;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return ui::kNotificationsIcon;
#if defined(OS_CHROMEOS)
    // TODO(xhwang): fix this icon, see crrev.com/863263007
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return kProductIcon;
#endif
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return ui::kMidiIcon;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return kExtensionIcon;
    default:
      NOTREACHED();
      return kExtensionIcon;
  }
#endif
}

base::string16 PermissionRequestImpl::GetMessageTextFragment() const {
  int message_id;
  switch (content_settings_type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
#if defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      message_id = IDS_FLASH_PERMISSION_FRAGMENT;
      break;
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringUTF16(message_id);
}

GURL PermissionRequestImpl::GetOrigin() const {
  return request_origin_;
}

void PermissionRequestImpl::PermissionGranted() {
  RegisterActionTaken();
  permission_decided_callback_.Run(persist(), CONTENT_SETTING_ALLOW);
}

void PermissionRequestImpl::PermissionDenied() {
  RegisterActionTaken();
  permission_decided_callback_.Run(persist(), CONTENT_SETTING_BLOCK);
}

void PermissionRequestImpl::Cancelled() {
  RegisterActionTaken();
  permission_decided_callback_.Run(false, CONTENT_SETTING_DEFAULT);
}

void PermissionRequestImpl::RequestFinished() {
  is_finished_ = true;
  delete_callback_.Run();
}

bool PermissionRequestImpl::ShouldShowPersistenceToggle() const {
  return (content_settings_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION) &&
         PermissionUtil::ShouldShowPersistenceToggle();
}

PermissionRequestType PermissionRequestImpl::GetPermissionRequestType()
    const {
  return PermissionUtil::GetRequestType(content_settings_type_);
}

PermissionRequestGestureType PermissionRequestImpl::GetGestureType()
    const {
  return PermissionUtil::GetGestureType(has_gesture_);
}
