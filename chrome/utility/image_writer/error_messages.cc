// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_writer/error_messages.h"

namespace image_writer {
namespace error {

const char kCleanUp[] = "Failed to clean up after write operation.";
const char kCloseDevice[] = "Failed to close usb device.";
const char kCloseImage[] = "Failed to close image file.";
const char kInvalidDevice[] = "Invalid device path.";
const char kNoOperationInProgress[] = "No operation in progress.";
const char kOpenDevice[] = "Failed to open device.";
const char kOpenImage[] = "Failed to open image.";
const char kOperationAlreadyInProgress[] = "Operation already in progress.";
const char kReadDevice[] = "Failed to read device.";
const char kReadImage[] = "Failed to read image.";
const char kWriteImage[] = "Writing image to device failed.";
const char kUnmountVolumes[] = "Unable to unmount the device.";
const char kVerificationFailed[] = "Verification failed.";

}  // namespace error
}  // namespace image_writer
