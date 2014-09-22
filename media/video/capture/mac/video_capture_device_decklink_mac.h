// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of VideoCaptureDevice class for Blackmagic video capture
// devices by using the DeckLink SDK.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_DECKLINK_MAC_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_DECKLINK_MAC_H_

#include "media/video/capture/video_capture_device.h"

#import <Foundation/Foundation.h>

namespace media {

// Extension of VideoCaptureDevice to create and manipulate Blackmagic devices
// via DeckLink SDK.
class MEDIA_EXPORT VideoCaptureDeviceDeckLinkMac : public VideoCaptureDevice {
 public:
  // Gets the names of all DeckLink video capture devices connected to this
  // computer, as enumerated by the DeckLink SDK.
  static void EnumerateDevices(VideoCaptureDevice::Names* device_names);

  // Gets the supported formats of a particular device attached to the system,
  // identified by |device|. Formats are retrieved from the DeckLink SDK.
  static void EnumerateDeviceCapabilities(
      const VideoCaptureDevice::Name& device,
      VideoCaptureFormats* supported_formats);

  explicit VideoCaptureDeviceDeckLinkMac(const Name& device_name);
  virtual ~VideoCaptureDeviceDeckLinkMac();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(
      const VideoCaptureParams& params,
      scoped_ptr<VideoCaptureDevice::Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceDeckLinkMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_DECKLINK_MAC_H_
