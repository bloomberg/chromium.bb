// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_menu_model.h"

#include <utility>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "content/public/common/media_stream_request.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

MediaStreamDevicesMenuModel::MediaStreamDevicesMenuModel(
    const MediaStreamInfoBarDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      selected_command_id_audio_(-1),
      selected_command_id_video_(-1) {
  const bool nonempty_audio_section =
      delegate->has_audio() && !delegate->GetAudioDevices().empty();
  if (delegate->has_video() && !delegate->GetVideoDevices().empty()) {
    // The default command ID is the first element that will be inserted.
    selected_command_id_video_ = commands_.size();
    AddDevices(delegate->GetVideoDevices());
    if (nonempty_audio_section)
      AddSeparator();
  }
  if (nonempty_audio_section) {
    // The default command ID is the first element that will be inserted.
    selected_command_id_audio_ = commands_.size();
    AddDevices(delegate->GetAudioDevices());
  }
}

MediaStreamDevicesMenuModel::~MediaStreamDevicesMenuModel() {
}

bool MediaStreamDevicesMenuModel::GetSelectedDeviceId(
    content::MediaStreamDeviceType type,
    std::string* device_id) const {
  int command_id = (type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE) ?
      selected_command_id_audio_ : selected_command_id_video_;
  CommandMap::const_iterator it = commands_.find(command_id);
  if (it != commands_.end())
    *device_id = it->second.device_id;
  return (it != commands_.end());
}

bool MediaStreamDevicesMenuModel::IsCommandIdChecked(int command_id) const {
  return (selected_command_id_audio_ == command_id ||
      selected_command_id_video_ == command_id);
}

bool MediaStreamDevicesMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool MediaStreamDevicesMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void MediaStreamDevicesMenuModel::ExecuteCommand(int command_id) {
  CommandMap::iterator it = commands_.find(command_id);
  DCHECK(it != commands_.end());

  if (it->second.type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE)
    selected_command_id_audio_ = command_id;
  else
    selected_command_id_video_ = command_id;
}

void MediaStreamDevicesMenuModel::AddDevices(
    const content::MediaStreamDevices& devices) {
  for (size_t i = 0; i < devices.size(); ++i) {
    int command_id = commands_.size();
    commands_.insert(std::make_pair(command_id, devices[i]));
    int message_id = (devices[i].type ==
        content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE) ?
        IDS_MEDIA_CAPTURE_MIC : IDS_MEDIA_CAPTURE_VIDEO;
    AddCheckItem(command_id,
                 l10n_util::GetStringFUTF16(message_id,
                                            UTF8ToUTF16(devices[i].name)));
  }
}
