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
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

enum DevicePermissionActions {
  kAllowHttps = 0,
  kAllowHttp,
  kDeny,
  kCancel,
  kPermissionActionsMax  // Must always be last!
};

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
      infobar_service->CreateConfirmInfoBar(
          std::unique_ptr<ConfirmInfoBarDelegate>(
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

bool MediaStreamInfoBarDelegateAndroid::IsRequestingVideoAccess() const {
  return controller_->IsAskingForVideo();
}

bool MediaStreamInfoBarDelegateAndroid::IsRequestingMicrophoneAccess() const {
  return controller_->IsAskingForAudio();
}

infobars::InfoBarDelegate::InfoBarIdentifier
MediaStreamInfoBarDelegateAndroid::GetIdentifier() const {
  return MEDIA_STREAM_INFOBAR_DELEGATE_ANDROID;
}

MediaStreamInfoBarDelegateAndroid::MediaStreamInfoBarDelegateAndroid(
    std::unique_ptr<MediaStreamDevicesController> controller)
    : ConfirmInfoBarDelegate(), controller_(std::move(controller)) {
  DCHECK(controller_.get());
  DCHECK(controller_->IsAskingForAudio() || controller_->IsAskingForVideo());
}

infobars::InfoBarDelegate::Type
MediaStreamInfoBarDelegateAndroid::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int MediaStreamInfoBarDelegateAndroid::GetIconId() const {
  return controller_->IsAskingForVideo() ? IDR_INFOBAR_MEDIA_STREAM_CAMERA
                                         : IDR_INFOBAR_MEDIA_STREAM_MIC;
}

void MediaStreamInfoBarDelegateAndroid::InfoBarDismissed() {
  // Deny the request if the infobar was closed with the 'x' button, since
  // we don't want WebRTC to be waiting for an answer that will never come.
  UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions", kCancel,
                            kPermissionActionsMax);
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
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_MEDIA_CAPTURE_ALLOW
                                       : IDS_MEDIA_CAPTURE_BLOCK);
}

bool MediaStreamInfoBarDelegateAndroid::Accept() {
  if (content::IsOriginSecure(controller_->GetOrigin())) {
    UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions", kAllowHttps,
                              kPermissionActionsMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions", kAllowHttp,
                              kPermissionActionsMax);
  }
  controller_->PermissionGranted();
  return true;
}

bool MediaStreamInfoBarDelegateAndroid::Cancel() {
  UMA_HISTOGRAM_ENUMERATION("Media.DevicePermissionActions", kDeny,
                            kPermissionActionsMax);
  controller_->PermissionDenied();
  return true;
}

base::string16 MediaStreamInfoBarDelegateAndroid::GetLinkText() const {
  return base::string16();
}

GURL MediaStreamInfoBarDelegateAndroid::GetLinkURL() const {
  return GURL(chrome::kMediaAccessLearnMoreUrl);
}
