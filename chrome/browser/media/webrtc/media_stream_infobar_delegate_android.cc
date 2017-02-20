// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_infobar_delegate_android.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

void DoNothing(bool update_content_setting, PermissionAction decision) {}

}  // namespace

// static
bool MediaStreamInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    bool user_gesture,
    std::unique_ptr<MediaStreamDevicesController> controller) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (!infobar_service) {
    // Deny the request if there is no place to show the infobar, e.g. when
    // the request comes from a background extension page.
    controller->Cancelled();
    return false;
  }

  std::unique_ptr<infobars::InfoBar> infobar(
      CreatePermissionInfoBar(std::unique_ptr<PermissionInfoBarDelegate>(
          new MediaStreamInfoBarDelegateAndroid(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              user_gesture,
              std::move(controller)))));
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* old_infobar = infobar_service->infobar_at(i);
    if (old_infobar->delegate()->AsMediaStreamInfoBarDelegateAndroid()) {
      infobar_service->ReplaceInfoBar(old_infobar, std::move(infobar));
      return true;
    }
  }
  infobar_service->AddInfoBar(std::move(infobar));
  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
MediaStreamInfoBarDelegateAndroid::GetIdentifier() const {
  return MEDIA_STREAM_INFOBAR_DELEGATE_ANDROID;
}

infobars::InfoBarDelegate::Type
MediaStreamInfoBarDelegateAndroid::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int MediaStreamInfoBarDelegateAndroid::GetIconId() const {
  return controller_->IsAskingForVideo() ? IDR_INFOBAR_MEDIA_STREAM_CAMERA
                                         : IDR_INFOBAR_MEDIA_STREAM_MIC;
}

MediaStreamInfoBarDelegateAndroid::MediaStreamInfoBarDelegateAndroid(
    Profile* profile,
    bool user_gesture,
    std::unique_ptr<MediaStreamDevicesController> controller)
    : PermissionInfoBarDelegate(
          controller->GetOrigin(),
          // The content setting type and permission type here are only passed
          // in to fit into PermissionInfoBarDelegate, even though media infobar
          // controls both mic and camera. This is a temporary state for easy
          // refactoring.
          // TODO(lshang): Merge MediaStreamInfoBarDelegateAndroid into
          // GroupedPermissionInfoBarDelegate. See crbug.com/606138.
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
          user_gesture,
          profile,
          // This is only passed in to fit into PermissionInfoBarDelegate.
          // Infobar accepted/denied/dismissed is handled by controller, not via
          // callbacks.
          base::Bind(&DoNothing)),
      controller_(std::move(controller)) {
  DCHECK(controller_.get());
  DCHECK(controller_->IsAskingForAudio() || controller_->IsAskingForVideo());
}

MediaStreamInfoBarDelegateAndroid::~MediaStreamInfoBarDelegateAndroid() {}

void MediaStreamInfoBarDelegateAndroid::InfoBarDismissed() {
  // Deny the request if the infobar was closed with the 'x' button, since
  // we don't want WebRTC to be waiting for an answer that will never come.
  controller_->Cancelled();
}

MediaStreamInfoBarDelegateAndroid*
MediaStreamInfoBarDelegateAndroid::AsMediaStreamInfoBarDelegateAndroid() {
  return this;
}

base::string16 MediaStreamInfoBarDelegateAndroid::GetMessageText() const {
  return controller_->GetMessageText();
}

base::string16 MediaStreamInfoBarDelegateAndroid::GetButtonLabel(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_MEDIA_CAPTURE_ALLOW);
    case BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(IDS_MEDIA_CAPTURE_BLOCK);
    default:
      NOTREACHED();
      return base::string16();
  }
}

int MediaStreamInfoBarDelegateAndroid::GetMessageResourceId() const {
  if (!controller_->IsAskingForAudio())
    return IDS_MEDIA_CAPTURE_VIDEO_ONLY;
  else if (!controller_->IsAskingForVideo())
    return IDS_MEDIA_CAPTURE_AUDIO_ONLY;
  return IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
}

bool MediaStreamInfoBarDelegateAndroid::Accept() {
  // TODO(dominickn): fold these metrics calls into
  // PermissionUmaUtil::PermissionGranted. See crbug.com/638076.
  PermissionUmaUtil::RecordPermissionPromptAccepted(
      controller_->GetPermissionRequestType(),
      PermissionUtil::GetGestureType(user_gesture()));

  bool persist_permission = true;
  if (ShouldShowPersistenceToggle()) {
    persist_permission = persist();

    if (controller_->IsAskingForAudio()) {
      PermissionUmaUtil::PermissionPromptAcceptedWithPersistenceToggle(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, persist_permission);
    }
    if (controller_->IsAskingForVideo()) {
      PermissionUmaUtil::PermissionPromptAcceptedWithPersistenceToggle(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, persist_permission);
    }
  }

  controller_->set_persist(persist_permission);
  controller_->PermissionGranted();
  return true;
}

bool MediaStreamInfoBarDelegateAndroid::Cancel() {
  // TODO(dominickn): fold these metrics calls into
  // PermissionUmaUtil::PermissionDenied. See crbug.com/638076.
  PermissionUmaUtil::RecordPermissionPromptDenied(
      controller_->GetPermissionRequestType(),
      PermissionUtil::GetGestureType(user_gesture()));

  bool persist_permission = true;
  if (ShouldShowPersistenceToggle()) {
    persist_permission = persist();

    if (controller_->IsAskingForAudio()) {
      PermissionUmaUtil::PermissionPromptDeniedWithPersistenceToggle(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, persist_permission);
    }
    if (controller_->IsAskingForVideo()) {
      PermissionUmaUtil::PermissionPromptDeniedWithPersistenceToggle(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, persist_permission);
    }
  }
  controller_->set_persist(persist_permission);
  controller_->PermissionDenied();
  return true;
}

base::string16 MediaStreamInfoBarDelegateAndroid::GetLinkText() const {
  return base::string16();
}

GURL MediaStreamInfoBarDelegateAndroid::GetLinkURL() const {
  return GURL(chrome::kMediaAccessLearnMoreUrl);
}

std::vector<int> MediaStreamInfoBarDelegateAndroid::content_settings_types()
    const {
  std::vector<int> types;
  if (controller_->IsAskingForAudio())
    types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  if (controller_->IsAskingForVideo())
    types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  return types;
}
