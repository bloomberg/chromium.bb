// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_stream_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

// TODO(xians): Register to the system monitor to get a device changed
// notification, update the selected devices based on the new device lists.
MediaStreamInfoBarDelegate::MediaStreamInfoBarDelegate(
    InfoBarTabHelper* tab_helper,
    MediaStreamDevicesController* controller)
    : InfoBarDelegate(tab_helper),
      controller_(controller),
      always_allow_(false) {
  if (HasAudio())
    selected_audio_device_ = GetAudioDevices().begin()->device_id;
  if (HasVideo())
    selected_video_device_ = GetVideoDevices().begin()->device_id;

  DCHECK(controller_.get());
}

MediaStreamInfoBarDelegate::~MediaStreamInfoBarDelegate() {}

bool MediaStreamInfoBarDelegate::HasAudio() const {
  return controller_->has_audio();
}

bool MediaStreamInfoBarDelegate::HasVideo() const {
  return controller_->has_video();
}

content::MediaStreamDevices
    MediaStreamInfoBarDelegate::GetAudioDevices() const {
  return controller_->GetAudioDevices();
}

content::MediaStreamDevices
    MediaStreamInfoBarDelegate::GetVideoDevices() const {
  return controller_->GetVideoDevices();
}

const std::string& MediaStreamInfoBarDelegate::GetSecurityOriginSpec() const {
  return controller_->GetSecurityOriginSpec();
}

bool MediaStreamInfoBarDelegate::IsSafeToAlwaysAllowAudio() const {
  return controller_->IsSafeToAlwaysAllowAudio();
}

bool MediaStreamInfoBarDelegate::IsSafeToAlwaysAllowVideo() const {
  return controller_->IsSafeToAlwaysAllowVideo();
}

void MediaStreamInfoBarDelegate::Accept() {
  DCHECK_NE(HasAudio(), selected_audio_device_.empty());
  DCHECK_NE(HasVideo(), selected_video_device_.empty());

  controller_->Accept(selected_audio_device_, selected_video_device_,
                      always_allow_);
}

void MediaStreamInfoBarDelegate::Deny() {
  controller_->Deny();
}

// MediaStreamInfoBarDelegate::CreateInfoBar is implemented in platform-specific
// files.

void MediaStreamInfoBarDelegate::InfoBarDismissed() {
  // Deny the request if the infobar was closed with the 'x' button, since
  // we don't want WebRTC to be waiting for an answer that will never come.
  Deny();
}

gfx::Image* MediaStreamInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(HasVideo() ?
      IDR_INFOBAR_MEDIA_STREAM_CAMERA : IDR_INFOBAR_MEDIA_STREAM_MIC);
}

InfoBarDelegate::Type MediaStreamInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

MediaStreamInfoBarDelegate*
    MediaStreamInfoBarDelegate::AsMediaStreamInfoBarDelegate() {
  return this;
}
