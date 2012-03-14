// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARAMS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARAMS_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"

#include <string>

#include "base/basictypes.h"

namespace gdata {

// Struct for response to ResumeUpload.
struct ResumeUploadResponse {
  ResumeUploadResponse(GDataErrorCode code,
                       int64 start_range_received,
                       int64 end_range_received,
                       const std::string& resource_id,
                       const std::string& md5_checksum);

  GDataErrorCode code;
  int64 start_range_received;
  int64 end_range_received;
  std::string resource_id;
  std::string md5_checksum;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARAMS_H_
