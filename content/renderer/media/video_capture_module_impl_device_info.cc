// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_module_impl_device_info.h"

#include "base/string_util.h"

static const char* kLocalDeviceName = "chromecamera";

VideoCaptureModuleImplDeviceInfo::VideoCaptureModuleImplDeviceInfo(
    const WebRtc_Word32 id) {
}

VideoCaptureModuleImplDeviceInfo::~VideoCaptureModuleImplDeviceInfo() {}

WebRtc_UWord32 VideoCaptureModuleImplDeviceInfo::NumberOfDevices() {
  return 1;
}

WebRtc_Word32 VideoCaptureModuleImplDeviceInfo::GetDeviceName(
    WebRtc_UWord32 device_number,
    WebRtc_UWord8* device_name_utf8,
    WebRtc_UWord32 device_name_length,
    WebRtc_UWord8* device_unique_id_utf8,
    WebRtc_UWord32 device_unique_id_utf8_length,
    WebRtc_UWord8* /* product_unique_id_utf8 */,
    WebRtc_UWord32 /* product_unique_id_utf8_length */) {
  base::strlcpy(reinterpret_cast<char*>(device_name_utf8),
          kLocalDeviceName, device_name_length);

  base::strlcpy(reinterpret_cast<char*>(device_unique_id_utf8),
          kLocalDeviceName, device_unique_id_utf8_length);

  return 0;
}

WebRtc_Word32 VideoCaptureModuleImplDeviceInfo::NumberOfCapabilities(
    const WebRtc_UWord8* deviceUniqueIdUTF8) {
  return 0;
}

WebRtc_Word32 VideoCaptureModuleImplDeviceInfo::GetCapability(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    const WebRtc_UWord32 deviceCapabilityNumber,
    webrtc::VideoCaptureCapability& capability) {
  return -1;
}

WebRtc_Word32 VideoCaptureModuleImplDeviceInfo::GetOrientation(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    webrtc::VideoCaptureRotation& orientation) {
  orientation = webrtc::kCameraRotate0;
  return -1;
}

WebRtc_Word32 VideoCaptureModuleImplDeviceInfo::GetBestMatchedCapability(
    const WebRtc_UWord8* deviceUniqueIdUTF8,
    const webrtc::VideoCaptureCapability requested,
    webrtc::VideoCaptureCapability& resulting) {
  return -1;
}

WebRtc_Word32 VideoCaptureModuleImplDeviceInfo::DisplayCaptureSettingsDialogBox(
    const WebRtc_UWord8* /* device_unique_id_utf8*/,
    const WebRtc_UWord8* /* dialog_title_utf8*/,
    void* /* parent_window */,
    WebRtc_UWord32 /* position_x */,
    WebRtc_UWord32 /* position_y */) {
  return -1;
}
