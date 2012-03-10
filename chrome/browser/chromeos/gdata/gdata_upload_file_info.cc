// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"

#include "base/string_number_conversions.h"

namespace gdata {

UploadFileInfo::UploadFileInfo()
    : file_size(0),
      content_length(0),
      file_stream(NULL),
      buf_len(0),
      start_range(0),
      end_range(-1),
      download_complete(false) {
}

UploadFileInfo::~UploadFileInfo() {
}

std::string UploadFileInfo::DebugString() const {
  return "title=[" + title +
         "], file_path=[" + file_path.value() +
         "], content_type=[" + content_type +
         "], file_size=[" + base::UintToString(file_size) +
         "], gdata_path=[" + gdata_path.value() +
         "]";
}

}  // namespace gdata
