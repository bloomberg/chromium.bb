// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_stream_infobar_delegate.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"

MediaStreamInfoBarDelegate::MediaStreamInfoBarDelegate(
    InfoBarTabHelper* tab_helper,
    MediaStreamDevicesController* controller)
    : InfoBarDelegate(tab_helper),
      controller_(controller) {
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

const GURL& MediaStreamInfoBarDelegate::GetSecurityOrigin() const {
  return controller_->GetSecurityOrigin();
}

void MediaStreamInfoBarDelegate::Accept(const std::string& audio_id,
                                        const std::string& video_id,
                                        bool always_allow) {
  controller_->Accept(audio_id, video_id, always_allow);
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
