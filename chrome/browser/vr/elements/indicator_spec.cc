// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"

namespace vr {

// clang-format off
std::vector<IndicatorSpec> GetIndicatorSpecs() {

  std::vector<IndicatorSpec> specs = {
      {kLocationAccessIndicator, kWebVrLocationAccessIndicator,
       kMyLocationIcon,
       IDS_VR_SHELL_SITE_IS_TRACKING_LOCATION,
       IDS_VR_SHELL_SITE_CAN_TRACK_LOCATION,
       &CapturingStateModel::location_access_enabled,
       &CapturingStateModel::location_access_potentially_enabled},

      {kAudioCaptureIndicator, kWebVrAudioCaptureIndicator,
       vector_icons::kMicIcon,
       IDS_VR_SHELL_SITE_IS_USING_MICROPHONE,
       IDS_VR_SHELL_SITE_CAN_USE_MICROPHONE,
       &CapturingStateModel::audio_capture_enabled,
       &CapturingStateModel::audio_capture_potentially_enabled},

      {kVideoCaptureIndicator, kWebVrVideoCaptureIndicator,
       vector_icons::kVideocamIcon,
       IDS_VR_SHELL_SITE_IS_USING_CAMERA,
       IDS_VR_SHELL_SITE_CAN_USE_CAMERA,
       &CapturingStateModel::video_capture_enabled,
       &CapturingStateModel::video_capture_potentially_enabled},

      {kBluetoothConnectedIndicator, kWebVrBluetoothConnectedIndicator,
       vector_icons::kBluetoothConnectedIcon,
       IDS_VR_SHELL_SITE_IS_USING_BLUETOOTH,
       IDS_VR_SHELL_SITE_CAN_USE_BLUETOOTH,
       &CapturingStateModel::bluetooth_connected,
       &CapturingStateModel::bluetooth_potentially_connected},

      {kScreenCaptureIndicator, kWebVrScreenCaptureIndicator,
       vector_icons::kScreenShareIcon,
       IDS_VR_SHELL_SITE_IS_SHARING_SCREEN,
       IDS_VR_SHELL_SITE_CAN_SHARE_SCREEN,
       &CapturingStateModel::screen_capture_enabled,
       &CapturingStateModel::screen_capture_potentially_enabled}};

  return specs;
}
// clang-format on

}  // namespace vr
