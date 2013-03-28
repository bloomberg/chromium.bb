// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_H_
#define CHROME_BROWSER_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_H_

#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/files/file_path.h"
#include "base/mac/cocoa_protocols.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"

// Clients use this listener interface to get notifications about
// events happening as a particular ImageCapture device is interacted with.
// Clients drive the interaction through the ImageCaptureDeviceManager
// and the ImageCaptureDevice classes, and get notifications of
// events through this interface.
class ImageCaptureDeviceListener {
 public:
  virtual ~ImageCaptureDeviceListener() {}

  // Get a notification that a particular item has been found on the device.
  // These calls will come automatically after a new device is initialized.
  virtual void ItemAdded(const std::string& name,
                         const base::PlatformFileInfo& info) = 0;

  // Called when there are no more items to retrieve.
  virtual void NoMoreItems() = 0;

  // Called upon completion of a file download request. The |path| is the
  // requested download file. Note: in NOT_FOUND error case, can be called
  // inline with the download request.
  virtual void DownloadedFile(const std::string& name,
                              base::PlatformFileError error) = 0;

  // Called to let the client know the device is removed. The client should
  // set the ImageCaptureDevice listener to null upon receiving this call.
  virtual void DeviceRemoved() = 0;
};

// Interface to a camera device found by ImageCaptureCore. This class manages a
// session to the camera and provides the backing interactions to present the
// media files on it to the filesystem delegate. FilePaths will be artificial,
// like "/$device_id/" + name.
// Note that all interactions with this class must happen on the UI thread.
@interface ImageCaptureDevice
    : NSObject<ICCameraDeviceDelegate, ICCameraDeviceDownloadDelegate> {
 @private
  scoped_nsobject<ICCameraDevice> camera_;
  base::WeakPtr<ImageCaptureDeviceListener> listener_;
}

- (id)initWithCameraDevice:(ICCameraDevice*)cameraDevice;
- (void)setListener:(base::WeakPtr<ImageCaptureDeviceListener>)listener;
- (void)open;
- (void)close;

// Download the given |file| to the provided |local_path|. Completion notice
// will be sent to the listener's DownloadedFile method.
- (void)downloadFile:(const std::string&)name
           localPath:(const base::FilePath&)localPath;

@end

#endif  // CHROME_BROWSER_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_H_
