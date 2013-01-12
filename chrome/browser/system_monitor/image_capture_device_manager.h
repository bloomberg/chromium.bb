// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_IMAGE_CAPTURE_DEVICE_MANAGER_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_IMAGE_CAPTURE_DEVICE_MANAGER_H_

#import <Foundation/Foundation.h>
#include <string>

#include "base/memory/scoped_nsobject.h"

@protocol ICDeviceBrowserDelegate;
@class ImageCaptureDevice;
@class ImageCaptureDeviceManagerImpl;

namespace chrome {

// Upon creation, begins monitoring for any attached devices using the
// ImageCapture API. Notifies clients of the presence of such devices
// (i.e. cameras,  USB cards) using the SystemMonitor and makes them
// available using |deviceForUUID|.
class ImageCaptureDeviceManager {
 public:
  ImageCaptureDeviceManager();
  ~ImageCaptureDeviceManager();

  // The UUIDs passed here are available in the device attach notifications
  // given through SystemMonitor. They're gotten by cracking the device ID
  // and taking the unique ID output.
  static ImageCaptureDevice* deviceForUUID(const std::string& uuid);

  // Returns a weak pointer to the internal ImageCapture interface protocol.
  id<ICDeviceBrowserDelegate> device_browser();

 private:
  scoped_nsobject<ImageCaptureDeviceManagerImpl> device_browser_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_IMAGE_CAPTURE_DEVICE_MANAGER_H_
