// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOAD_ERROR_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOAD_ERROR_H_

namespace google_apis {

// Represents errors on uploading a file to Drive.
enum DriveUploadError {
  DRIVE_UPLOAD_OK,
  DRIVE_UPLOAD_ERROR_NOT_FOUND,
  DRIVE_UPLOAD_ERROR_NO_SPACE,
  DRIVE_UPLOAD_ERROR_CONFLICT,
  DRIVE_UPLOAD_ERROR_ABORT,
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOAD_ERROR_H_
