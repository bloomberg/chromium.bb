// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// DirectShow is used for capturing. DirectShow provide its own threads
// for capturing.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_

#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdint.h>
#include <wrl/client.h>

#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"

interface IMFSourceReader;

namespace base {
class Location;
}  // namespace base

namespace media {

class MFReaderCallback;

const DWORD kFirstVideoStream =
    static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

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

  // Captured new video data.
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              int rotation,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp);

 private:
  void OnError(const base::Location& from_here, HRESULT hr);

  VideoCaptureDeviceDescriptor descriptor_;
  Microsoft::WRL::ComPtr<IMFActivate> device_;
  scoped_refptr<MFReaderCallback> callback_;

  base::Lock lock_;  // Used to guard the below variables.
  std::unique_ptr<VideoCaptureDevice::Client> client_;
  Microsoft::WRL::ComPtr<IMFSourceReader> reader_;
  VideoCaptureFormat capture_format_;
  bool capture_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceMFWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
