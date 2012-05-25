// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>

#include "base/logging.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "content/public/common/media_stream_request.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// A predicate that checks if a StreamDeviceInfo object has the same ID as the
// device ID specified at construction.
class DeviceIdEquals {
 public:
  explicit DeviceIdEquals(const std::string& device_id)
      : device_id_(device_id) {
  }

  bool operator() (const content::MediaStreamDevice& device) {
    return device.device_id == device_id_;
  }

 private:
  std::string device_id_;
};

}  // namespace

MediaStreamInfoBarDelegate::MediaStreamInfoBarDelegate(
    InfoBarTabHelper* tab_helper,
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback)
    : InfoBarDelegate(tab_helper),
      request_(request),
      callback_(callback) {
  DCHECK(request_);
  has_audio_ = request_->devices.count(
      content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE) != 0;
  has_video_ = request_->devices.count(
      content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE) != 0;
}

MediaStreamInfoBarDelegate::~MediaStreamInfoBarDelegate() {
}

content::MediaStreamDevices
    MediaStreamInfoBarDelegate::GetAudioDevices() const {
  if (!has_audio_)
    return content::MediaStreamDevices();
  content::MediaStreamDeviceMap::const_iterator it =
      request_->devices.find(content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE);
  DCHECK(it != request_->devices.end());
  return it->second;
}

content::MediaStreamDevices
    MediaStreamInfoBarDelegate::GetVideoDevices() const {
  if (!has_video_)
    return content::MediaStreamDevices();
  content::MediaStreamDeviceMap::const_iterator it =
      request_->devices.find(content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE);
  DCHECK(it != request_->devices.end());
  return it->second;
}

const GURL& MediaStreamInfoBarDelegate::GetSecurityOrigin() const {
  return request_->security_origin;
}

void MediaStreamInfoBarDelegate::Accept(const std::string& audio_id,
                                        const std::string& video_id) {
  content::MediaStreamDevices devices;

  if (has_audio_) {
    AddDeviceWithId(content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE,
                    audio_id, &devices);
  }
  if (has_video_) {
    AddDeviceWithId(content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE,
                    video_id, &devices);
  }

  callback_.Run(devices);
}

void MediaStreamInfoBarDelegate::Deny() {
  callback_.Run(content::MediaStreamDevices());
}

void MediaStreamInfoBarDelegate::AddDeviceWithId(
    content::MediaStreamDeviceType type,
    const std::string& id,
    content::MediaStreamDevices* devices) {
  DCHECK(devices);
  content::MediaStreamDeviceMap::const_iterator device_it =
      request_->devices.find(type);
  if (device_it != request_->devices.end()) {
    content::MediaStreamDevices::const_iterator it = std::find_if(
        device_it->second.begin(), device_it->second.end(), DeviceIdEquals(id));
    if (it != device_it->second.end())
      devices->push_back(*it);
  }
}

// MediaStreamInfoBarDelegate::CreateInfoBar is implemented in platform-specific
// files.

void MediaStreamInfoBarDelegate::InfoBarDismissed() {
  // Deny the request if the infobar was closed with the 'x' button, since
  // we don't want WebRTC to be waiting for an answer that will never come.
  Deny();
}

gfx::Image* MediaStreamInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(has_video_ ?
      IDR_INFOBAR_MEDIA_STREAM_CAMERA : IDR_INFOBAR_MEDIA_STREAM_MIC);
}

InfoBarDelegate::Type MediaStreamInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

MediaStreamInfoBarDelegate*
    MediaStreamInfoBarDelegate::AsMediaStreamInfoBarDelegate() {
  return this;
}
