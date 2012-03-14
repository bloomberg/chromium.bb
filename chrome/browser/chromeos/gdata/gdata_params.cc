// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_params.h"

namespace gdata {

ResumeUploadResponse::ResumeUploadResponse(GDataErrorCode code,
                                         int64 start_range_received,
                                         int64 end_range_received,
                                         const std::string& resource_id,
                                         const std::string& md5_checksum) :
    code(code),
    start_range_received(start_range_received),
    end_range_received(end_range_received),
    resource_id(resource_id),
    md5_checksum(md5_checksum) {
}

}  // namespace gdata
