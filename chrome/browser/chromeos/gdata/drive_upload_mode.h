// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOAD_MODE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOAD_MODE_H_

namespace gdata {

// The mode for uploading.
enum UploadMode {
  UPLOAD_NEW_FILE,
  UPLOAD_EXISTING_FILE,
  UPLOAD_INVALID,  // Used as an invalid value.
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOAD_MODE_H_
