// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_upload_file_info.h"

#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "net/base/file_stream.h"

namespace gdata {

UploadFileInfo::UploadFileInfo()
    : upload_id(-1),
      file_size(0),
      content_length(0),
      upload_mode(UPLOAD_INVALID),
      file_stream(NULL),
      buf_len(0),
      start_range(0),
      end_range(-1),
      all_bytes_present(false),
      upload_paused(false),
      should_retry_file_open(false),
      num_file_open_tries(0) {
}

UploadFileInfo::~UploadFileInfo() {
  // The file stream is closed by the destructor asynchronously.
  if (file_stream) {
    delete file_stream;
    file_stream = NULL;
  }
}

int64 UploadFileInfo::SizeRemaining() const {
  DCHECK(file_size > end_range);
  // Note that uploaded_bytes = end_range + 1;
  return file_size - end_range - 1;
}

std::string UploadFileInfo::DebugString() const {
  return "title=[" + title +
         "], file_path=[" + file_path.value() +
         "], content_type=[" + content_type +
         "], file_size=[" + base::UintToString(file_size) +
         "], drive_path=[" + drive_path.value() +
         "]";
}

}  // namespace gdata
