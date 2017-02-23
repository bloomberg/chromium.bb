// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_infobar_delegate.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/geolocation/geolocation_infobar_delegate_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/midi_permission_infobar_delegate_android.h"
#include "chrome/browser/media/protected_media_identifier_infobar_delegate_android.h"
#include "chrome/browser/notifications/notification_permission_infobar_delegate.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* PermissionInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    ContentSettingsType type,
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback) {
  std::unique_ptr<PermissionInfoBarDelegate> delegate =
      PermissionInfoBarDelegate::CreateDelegate(
          type, requesting_frame, user_gesture, profile, callback);

  if (!delegate)
    return nullptr;

  return infobar_service->AddInfoBar(
      CreatePermissionInfoBar(std::move(delegate)));
}

// static
std::unique_ptr<PermissionInfoBarDelegate>
PermissionInfoBarDelegate::CreateDelegate(
    ContentSettingsType type,
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return std::unique_ptr<PermissionInfoBarDelegate>(
              new GeolocationInfoBarDelegateAndroid(
                  requesting_frame, user_gesture, profile, callback));
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return std::unique_ptr<PermissionInfoBarDelegate>(
          new NotificationPermissionInfoBarDelegate(
              type, requesting_frame, user_gesture, profile, callback));
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return std::unique_ptr<PermissionInfoBarDelegate>(
              new MidiPermissionInfoBarDelegateAndroid(
                  requesting_frame, user_gesture, profile, callback));
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return std::unique_ptr<PermissionInfoBarDelegate>(
              new ProtectedMediaIdentifierInfoBarDelegateAndroid(
                  requesting_frame, user_gesture, profile, callback));
    default:
      NOTREACHED();
      return nullptr;
  }
}

PermissionInfoBarDelegate::~PermissionInfoBarDelegate() {
  if (!action_taken_) {
    PermissionUmaUtil::PermissionIgnored(
        content_settings_type_,
        user_gesture_ ? PermissionRequestGestureType::GESTURE
                      : PermissionRequestGestureType::NO_GESTURE,
        requesting_origin_, profile_);

    PermissionUmaUtil::RecordEmbargoStatus(
        PermissionEmbargoStatus::NOT_EMBARGOED);
  }
}

std::vector<int> PermissionInfoBarDelegate::content_settings_types() const {
  return std::vector<int>{content_settings_type_};
}

bool PermissionInfoBarDelegate::ShouldShowPersistenceToggle() const {
  return (content_settings_type_ == CONTENT_SETTINGS_TYPE_GEOLOCATION ||
          content_settings_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
          content_settings_type_ == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) &&
         PermissionUtil::ShouldShowPersistenceToggle();
}

bool PermissionInfoBarDelegate::Accept() {
  bool update_content_setting = true;
  if (ShouldShowPersistenceToggle()) {
    update_content_setting = persist_;
    PermissionUmaUtil::PermissionPromptAcceptedWithPersistenceToggle(
        content_settings_type_, persist_);
  }

  SetPermission(update_content_setting, PermissionAction::GRANTED);
  return true;
}

bool PermissionInfoBarDelegate::Cancel() {
  bool update_content_setting = true;
  if (ShouldShowPersistenceToggle()) {
    update_content_setting = persist_;
    PermissionUmaUtil::PermissionPromptDeniedWithPersistenceToggle(
        content_settings_type_, persist_);
  }

  SetPermission(update_content_setting, PermissionAction::DENIED);
  return true;
}

void PermissionInfoBarDelegate::InfoBarDismissed() {
  SetPermission(false, PermissionAction::DISMISSED);
}

base::string16 PermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_PERMISSION_ALLOW
                                                         : IDS_PERMISSION_DENY);
}

base::string16 PermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      GetMessageResourceId(),
      url_formatter::FormatUrlForSecurityDisplay(
          requesting_origin_,
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

PermissionInfoBarDelegate::PermissionInfoBarDelegate(
    const GURL& requesting_origin,
    ContentSettingsType content_settings_type,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback)
    : requesting_origin_(requesting_origin),
      content_settings_type_(content_settings_type),
      profile_(profile),
      callback_(callback),
      action_taken_(false),
      user_gesture_(user_gesture),
      persist_(true) {}

infobars::InfoBarDelegate::Type PermissionInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

PermissionInfoBarDelegate*
PermissionInfoBarDelegate::AsPermissionInfoBarDelegate() {
  return this;
}

void PermissionInfoBarDelegate::SetPermission(bool update_content_setting,
                                              PermissionAction decision) {
  action_taken_ = true;
  callback_.Run(update_content_setting, decision);
}
