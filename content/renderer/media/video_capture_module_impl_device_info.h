// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MODULE_IMPL_DEVICE_INFO_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MODULE_IMPL_DEVICE_INFO_H_

#include "base/basictypes.h"
#include "third_party/webrtc/modules/video_capture/main/interface/video_capture.h"

// An implementation of webrtc::VideoCaptureModule::DeviceInfo returns device
// information about video capture on Chrome platform. Actually, this is a dummy
// class. The real device management is done by media stream on Chrome.
class VideoCaptureModuleImplDeviceInfo
    : public webrtc::VideoCaptureModule::DeviceInfo {
 public:
  explicit VideoCaptureModuleImplDeviceInfo(const WebRtc_Word32 id);
  virtual ~VideoCaptureModuleImplDeviceInfo();

  // webrtc::VideoCaptureModule::DeviceInfo implementation.
  virtual WebRtc_UWord32 NumberOfDevices();
  virtual WebRtc_Word32 GetDeviceName(
      WebRtc_UWord32 device_number,
      WebRtc_UWord8* device_name_utf8,
      WebRtc_UWord32 device_name_length,
      WebRtc_UWord8* device_unique_id_utf8,
      WebRtc_UWord32 device_unique_id_utf8_ength,
      WebRtc_UWord8* product_unique_id_utf8 = 0,
      WebRtc_UWord32 product_unique_id_utf8_length = 0);
  virtual WebRtc_Word32 NumberOfCapabilities(
      const WebRtc_UWord8* deviceUniqueIdUTF8);
  virtual WebRtc_Word32 GetCapability(
      const WebRtc_UWord8* deviceUniqueIdUTF8,
      const WebRtc_UWord32 deviceCapabilityNumber,
      webrtc::VideoCaptureCapability& capability);
  virtual WebRtc_Word32 GetOrientation(
      const WebRtc_UWord8* deviceUniqueIdUTF8,
      webrtc::VideoCaptureRotation& orientation);
  virtual WebRtc_Word32 GetBestMatchedCapability(
      const WebRtc_UWord8* deviceUniqueIdUTF8,
      const webrtc::VideoCaptureCapability requested,
      webrtc::VideoCaptureCapability& resulting);
  virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(
      const WebRtc_UWord8* device_unique_id_utf8,
      const WebRtc_UWord8* dialog_title_utf8,
      void* parent_window,
      WebRtc_UWord32 position_x,
      WebRtc_UWord32 position_y);

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureModuleImplDeviceInfo);
};

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MODULE_IMPL_DEVICE_INFO_H_
