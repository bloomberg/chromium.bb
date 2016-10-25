// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_infobar_delegate.h"

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
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* PermissionInfoBarDelegate::Create(
    content::PermissionType type,
    InfoBarService* infobar_service,
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback) {
  switch (type) {
    case content::PermissionType::GEOLOCATION:
      return infobar_service->AddInfoBar(
          CreatePermissionInfoBar(std::unique_ptr<PermissionInfoBarDelegate>(
              new GeolocationInfoBarDelegateAndroid(
                  requesting_frame, user_gesture, profile, callback))));
#if defined(ENABLE_NOTIFICATIONS)
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      return infobar_service->AddInfoBar(
          CreatePermissionInfoBar(std::unique_ptr<PermissionInfoBarDelegate>(
              new NotificationPermissionInfoBarDelegate(
                  requesting_frame, user_gesture, profile, callback))));
#endif  // ENABLE_NOTIFICATIONS
    case content::PermissionType::MIDI_SYSEX:
      return infobar_service->AddInfoBar(
          CreatePermissionInfoBar(std::unique_ptr<PermissionInfoBarDelegate>(
              new MidiPermissionInfoBarDelegateAndroid(
                  requesting_frame, user_gesture, profile, callback))));
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return infobar_service->AddInfoBar(
          CreatePermissionInfoBar(std::unique_ptr<PermissionInfoBarDelegate>(
              new ProtectedMediaIdentifierInfoBarDelegateAndroid(
                  requesting_frame, user_gesture, profile, callback))));
    default:
      NOTREACHED();
      return nullptr;
  }
}

PermissionInfoBarDelegate::~PermissionInfoBarDelegate() {
  if (!action_taken_) {
    PermissionUmaUtil::PermissionIgnored(
        permission_type_,
        user_gesture_ ? PermissionRequestGestureType::GESTURE
                      : PermissionRequestGestureType::NO_GESTURE,
        requesting_origin_, profile_);
  }
}

std::vector<int> PermissionInfoBarDelegate::content_settings() const {
  return std::vector<int>{content_settings_type_};
}

bool PermissionInfoBarDelegate::ShouldShowPersistenceToggle() const {
  return (permission_type_ == content::PermissionType::GEOLOCATION ||
          permission_type_ == content::PermissionType::AUDIO_CAPTURE ||
          permission_type_ == content::PermissionType::VIDEO_CAPTURE) &&
         PermissionUtil::ShouldShowPersistenceToggle();
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

infobars::InfoBarDelegate::Type PermissionInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

void PermissionInfoBarDelegate::InfoBarDismissed() {
  SetPermission(false, DISMISSED);
}

PermissionInfoBarDelegate*
PermissionInfoBarDelegate::AsPermissionInfoBarDelegate() {
  return this;
}

base::string16 PermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PERMISSION_ALLOW : IDS_PERMISSION_DENY);
}

bool PermissionInfoBarDelegate::Accept() {
  bool update_content_setting = true;
  if (ShouldShowPersistenceToggle()) {
    update_content_setting = persist_;
    PermissionUmaUtil::PermissionPromptAcceptedWithPersistenceToggle(
        permission_type_, persist_);
  }

  SetPermission(update_content_setting, GRANTED);
  return true;
}

bool PermissionInfoBarDelegate::Cancel() {
  bool update_content_setting = true;
  if (ShouldShowPersistenceToggle()) {
    update_content_setting = persist_;
    PermissionUmaUtil::PermissionPromptDeniedWithPersistenceToggle(
        permission_type_, persist_);
  }

  SetPermission(update_content_setting, DENIED);
  return true;
}

void PermissionInfoBarDelegate::SetPermission(bool update_content_setting,
                                              PermissionAction decision) {
  action_taken_ = true;
  callback_.Run(update_content_setting, decision);
}
