// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_media_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

ContentSettingMediaMenuModel::ContentSettingMediaMenuModel(
    content::MediaStreamType type,
    ContentSettingBubbleModel* bubble_model,
    const MenuLabelChangedCallback& callback)
    : ui::SimpleMenuModel(this),
      type_(type),
      media_bubble_model_(bubble_model),
      callback_(callback) {
  DCHECK(type_ == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
         type_ == content::MEDIA_DEVICE_VIDEO_CAPTURE);
  DCHECK_EQ(CONTENT_SETTINGS_TYPE_MEDIASTREAM,
            media_bubble_model_->content_type());
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  const content::MediaStreamDevices& devices =
      (type_ == content::MEDIA_DEVICE_AUDIO_CAPTURE) ?
          dispatcher->GetAudioCaptureDevices() :
          dispatcher->GetVideoCaptureDevices();

  for (size_t i = 0; i < devices.size(); ++i) {
    commands_.insert(std::make_pair(commands_.size(), devices[i]));
    AddItem(i, base::UTF8ToUTF16(devices[i].name));
  }
}

ContentSettingMediaMenuModel::~ContentSettingMediaMenuModel() {
}

bool ContentSettingMediaMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ContentSettingMediaMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ContentSettingMediaMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ContentSettingMediaMenuModel::ExecuteCommand(int command_id,
                                                  int event_flags) {
  CommandMap::const_iterator it = commands_.find(command_id);
  DCHECK(it != commands_.end());
  media_bubble_model_->OnMediaMenuClicked(type_, it->second.id);

  if (!callback_.is_null())
    callback_.Run(type_, it->second.name);
}
