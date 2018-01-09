// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// MediaFoundation is used for capturing. MediaFoundation provides its own
// threads for capturing.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_

#include <mfcaptureengine.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdint.h>
#include <wrl/client.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"

interface IMFSourceReader;

namespace base {
class Location;
}  // namespace base

namespace media {

class MFVideoCallback;

class CAPTURE_EXPORT VideoCaptureDeviceMFWin : public VideoCaptureDevice {
 public:
  static bool FormatFromGuid(const GUID& guid, VideoPixelFormat* format);

  explicit VideoCaptureDeviceMFWin(
      const VideoCaptureDeviceDescriptor& device_descriptor);
  ~VideoCaptureDeviceMFWin() override;

  // Opens the device driver for this device.
  bool Init(const Microsoft::WRL::ComPtr<IMFMediaSource>& source);

  // VideoCaptureDevice implementation.
  void AllocateAndStart(
      const VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

  // Captured new video data.
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              int rotation,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp);
  void OnEvent(IMFMediaEvent* media_event);

  using CreateMFPhotoCallbackCB =
      base::RepeatingCallback<scoped_refptr<IMFCaptureEngineOnSampleCallback>(
          VideoCaptureDevice::TakePhotoCallback callback,
          VideoCaptureFormat format)>;

  bool get_use_photo_stream_to_take_photo_for_testing() {
    return use_photo_stream_to_take_photo_;
  }

  void set_create_mf_photo_callback_for_testing(CreateMFPhotoCallbackCB cb) {
    create_mf_photo_callback_ = cb;
  }

 private:
  void OnError(const base::Location& from_here, HRESULT hr);

  VideoCaptureDeviceDescriptor descriptor_;
  CreateMFPhotoCallbackCB create_mf_photo_callback_;
  scoped_refptr<MFVideoCallback> video_callback_;

  // Guards the below variables from concurrent access between methods running
  // on |sequence_checker_| and calls to OnIncomingCapturedData() and OnEvent()
  // made by MediaFoundation on threads outside of our control.
  base::Lock lock_;

  std::unique_ptr<VideoCaptureDevice::Client> client_;
  Microsoft::WRL::ComPtr<IMFCaptureEngine> engine_;
  const GUID sink_mf_video_format_;
  const VideoPixelFormat sink_video_pixel_format_;
  VideoCaptureFormat sink_capture_video_format_;
  bool is_started_;
  DWORD video_stream_index_;
  DWORD photo_stream_index_;
  bool use_photo_stream_to_take_photo_;
  base::queue<TakePhotoCallback> video_stream_take_photo_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceMFWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
