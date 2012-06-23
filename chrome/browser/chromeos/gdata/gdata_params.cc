// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_params.h"

namespace gdata {

ResumeUploadResponse::ResumeUploadResponse(GDataErrorCode code,
                                           int64 start_range_received,
                                           int64 end_range_received)
    : code(code),
      start_range_received(start_range_received),
      end_range_received(end_range_received) {
}

ResumeUploadResponse::~ResumeUploadResponse() {
}

InitiateUploadParams::InitiateUploadParams(
    UploadMode upload_mode,
    const std::string& title,
    const std::string& content_type,
    int64 content_length,
    const GURL& upload_location,
    const FilePath& virtual_path)
    : upload_mode(upload_mode),
      title(title),
      content_type(content_type),
      content_length(content_length),
      upload_location(upload_location),
      virtual_path(virtual_path) {
}

InitiateUploadParams::~InitiateUploadParams() {
}

ResumeUploadParams::ResumeUploadParams(
    UploadMode upload_mode,
    int64 start_range,
    int64 end_range,
    int64 content_length,
    const std::string& content_type,
    scoped_refptr<net::IOBuffer> buf,
    const GURL& upload_location,
    const FilePath& virtual_path) : upload_mode(upload_mode),
                                    start_range(start_range),
                                    end_range(end_range),
                                    content_length(content_length),
                                    content_type(content_type),
                                    buf(buf),
                                    upload_location(upload_location),
                                    virtual_path(virtual_path) {
}

ResumeUploadParams::~ResumeUploadParams() {
}

}  // namespace gdata
