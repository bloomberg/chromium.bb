// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"

namespace extensions {
namespace image_writer {
namespace error {

const char kAborted[] = "Aborted.";
const char kCloseDevice[] = "Failed to close usb device file.";
const char kCloseImage[] = "Failed to close image file.";
const char kDeviceList[] = "Failed to retrieve device list.";
const char kDeviceMD5[] = "Failed to calculate MD5 sum for written image.";
const char kDownloadCancelled[] = "The download was cancelled.";
const char kDownloadHash[] = "Downloaded image had incorrect MD5 sum.";
const char kDownloadInterrupted[] = "Download interrupted.";
const char kDownloadMD5[] = "Failed to calculate MD5 sum for download.";
const char kEmptyUnzip[] = "Unzipped image contained no files.";
const char kFileOperationsNotImplemented[] = "Write-from file not implemented.";
const char kImageBurnerError[] = "Error contacting Image Burner process.";
const char kImageMD5[] = "Failed to calculate MD5 sum for unzipped image.";
const char kImageNotFound[] = "Unpacked image not found.";
const char kImageSize[] = "Could not determine image size.";
const char kInvalidUrl[] = "Invalid URL provided.";
const char kMultiFileZip[] = "More than one file in zip."
                             " Unsure how to proceed.";
const char kNoOperationInProgress[] = "No operation in progress.";
const char kOpenDevice[] = "Failed to open usb device for writing.";
const char kOpenImage[] = "Failed to open image.";
const char kOperationAlreadyInProgress[] = "Operation already in progress.";
const char kPrematureEndOfFile[] = "Premature end of image file.";
const char kReadImage[] = "Failed to read image.";
const char kTempDir[] = "Unable to create temporary directory.";
const char kTempFile[] = "Unable to create temporary file.";
const char kUnsupportedOperation[] = "Unsupported operation.";
const char kUnzip[] = "Unzip failed.";
const char kWriteHash[] = "Writing image resulted in incorrect hash.";
const char kWriteImage[] = "Writing image to device failed.";

} // namespace error
} // namespace image_writer
} // namespace extensions
