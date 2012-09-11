// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_devices_menu_model.h"

#include <utility>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "content/public/common/media_stream_request.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

MediaStreamDevicesMenuModel::MediaStreamDevicesMenuModel(
    MediaStreamInfoBarDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      media_stream_delegate_(delegate) {
  if (delegate->HasVideo()) {
    AddDevices(delegate->GetVideoDevices());
    if (delegate->HasAudio())
      AddSeparator(ui::NORMAL_SEPARATOR);
  }
  if (delegate->HasAudio()) {
    AddDevices(delegate->GetAudioDevices());
  }

  // Show "always allow" option when auto-accepting would be safe in the future.
  AddAlwaysAllowOption(
      delegate->HasAudio() && delegate->IsSafeToAlwaysAllowAudio(),
      delegate->HasVideo() && delegate->IsSafeToAlwaysAllowVideo());
}

MediaStreamDevicesMenuModel::~MediaStreamDevicesMenuModel() {
}

bool MediaStreamDevicesMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_MEDIA_STREAM_DEVICE_ALWAYS_ALLOW)
    return media_stream_delegate_->always_allow();

  CommandMap::const_iterator it = commands_.find(command_id);
  DCHECK(it != commands_.end());

  const std::string& device_id = it->second.device_id;
  return (device_id == media_stream_delegate_->selected_audio_device() ||
          device_id == media_stream_delegate_->selected_video_device());
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
  if (command_id == IDC_MEDIA_STREAM_DEVICE_ALWAYS_ALLOW) {
    media_stream_delegate_->toggle_always_allow();
    return;
  }

  CommandMap::const_iterator it = commands_.find(command_id);
  DCHECK(it != commands_.end());
  if (content::IsAudioMediaType(it->second.type)) {
    media_stream_delegate_->set_selected_audio_device(it->second.device_id);
  } else if (content::IsVideoMediaType(it->second.type)) {
    media_stream_delegate_->set_selected_video_device(it->second.device_id);
  } else {
    NOTREACHED();
  }
}

void MediaStreamDevicesMenuModel::AddDevices(
    const content::MediaStreamDevices& devices) {
  for (size_t i = 0; i < devices.size(); ++i) {
    int command_id = commands_.size();
    commands_.insert(std::make_pair(command_id, devices[i]));
    int message_id;
    if (content::IsAudioMediaType(devices[i].type)) {
      message_id = IDS_MEDIA_CAPTURE_AUDIO;
    } else if (content::IsVideoMediaType(devices[i].type)) {
      message_id = IDS_MEDIA_CAPTURE_VIDEO;
    } else {
      NOTIMPLEMENTED();
      continue;
    }
    AddCheckItem(command_id,
                 l10n_util::GetStringFUTF16(message_id,
                                            UTF8ToUTF16(devices[i].name)));
  }
}

void MediaStreamDevicesMenuModel::AddAlwaysAllowOption(bool audio, bool video) {
  if (!audio && !video)
    return;

  int command_id = IDC_MEDIA_STREAM_DEVICE_ALWAYS_ALLOW;
  int message_id = IDS_MEDIA_CAPTURE_ALWAYS_ALLOW_AUDIO_AND_VIDEO;
  if (audio && !video)
    message_id = IDS_MEDIA_CAPTURE_ALWAYS_ALLOW_AUDIO_ONLY;
  else if (!audio && video)
    message_id = IDS_MEDIA_CAPTURE_ALWAYS_ALLOW_VIDEO_ONLY;

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddCheckItem(command_id, l10n_util::GetStringUTF16(message_id));
}
