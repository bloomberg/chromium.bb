// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_infobar_delegate_android.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const int kGroupedInfobarAudioPosition = 0;
const int kGroupedInfobarVideoPosition = 1;

// Get a list of content types being requested. Note that the order of the
// resulting array corresponds to the kGroupedInfobarAudio/VideoPermission
// constants.
std::vector<ContentSettingsType> GetContentSettingsTypes(
    MediaStreamDevicesController* controller) {
  std::vector<ContentSettingsType> types;
  if (controller->IsAskingForAudio())
    types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  if (controller->IsAskingForVideo())
    types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  return types;
}

}  // namespace

MediaStreamInfoBarDelegateAndroid::~MediaStreamInfoBarDelegateAndroid() {}

// static
bool MediaStreamInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
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
      GroupedPermissionInfoBarDelegate::CreateInfoBar(infobar_service,
          std::unique_ptr<GroupedPermissionInfoBarDelegate>(
              new MediaStreamInfoBarDelegateAndroid(std::move(controller)))));
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

MediaStreamInfoBarDelegateAndroid::MediaStreamInfoBarDelegateAndroid(
    std::unique_ptr<MediaStreamDevicesController> controller)
    : GroupedPermissionInfoBarDelegate(
          controller->GetOrigin(),
          GetContentSettingsTypes(controller.get())),
      controller_(std::move(controller)) {
  DCHECK(controller_.get());
  DCHECK(controller_->IsAskingForAudio() || controller_->IsAskingForVideo());
}

void MediaStreamInfoBarDelegateAndroid::InfoBarDismissed() {
  // Deny the request if the infobar was closed with the 'x' button, since
  // we don't want WebRTC to be waiting for an answer that will never come.
  controller_->Cancelled();
}

MediaStreamInfoBarDelegateAndroid*
MediaStreamInfoBarDelegateAndroid::AsMediaStreamInfoBarDelegateAndroid() {
  return this;
}

bool MediaStreamInfoBarDelegateAndroid::Accept() {
  if (GetPermissionCount() == 2) {
    controller_->GroupedRequestFinished(
        GetAcceptState(kGroupedInfobarAudioPosition),
        GetAcceptState(kGroupedInfobarVideoPosition));
  } else {
    DCHECK_EQ(1, GetPermissionCount());
    controller_->PermissionGranted();
  }
  return true;
}

bool MediaStreamInfoBarDelegateAndroid::Cancel() {
  controller_->PermissionDenied();
  return true;
}
