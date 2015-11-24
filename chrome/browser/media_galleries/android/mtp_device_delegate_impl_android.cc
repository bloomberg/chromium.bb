// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"

void CreateMTPDeviceAsyncDelegate(
    const std::string& device_location,
    const bool read_only,
    const CreateMTPDeviceAsyncDelegateCallback& callback) {
  // Return nullptr as there is no MTPDeviceAsyncDelegate implementation for
  // Chrome on Android at the moment.
  // TODO(crbug.com/560390): Add an MTPDeviceAsyncDelegate implementation on
  // Android.
  NOTIMPLEMENTED();
  callback.Run(nullptr);
}
