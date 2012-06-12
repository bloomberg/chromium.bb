// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARAMS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARAMS_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "net/base/io_buffer.h"
#include "googleurl/src/gurl.h"

namespace gdata {

class GDataEntry;

// Struct for response to ResumeUpload.
struct ResumeUploadResponse {
  ResumeUploadResponse(GDataErrorCode code,
                       int64 start_range_received,
                       int64 end_range_received);
  ~ResumeUploadResponse();

  GDataErrorCode code;
  int64 start_range_received;
  int64 end_range_received;
  FilePath virtual_path;
};

// Struct for passing params needed for DocumentsService::ResumeUpload() calls.
struct ResumeUploadParams {
  ResumeUploadParams(const std::string& title,
                     int64 start_range,
                     int64 end_range,
                     int64 content_length,
                     const std::string& content_type,
                     scoped_refptr<net::IOBuffer> buf,
                     const GURL& upload_location,
                     const FilePath& virtual_path);
  ~ResumeUploadParams();

  std::string title;  // Title to be used for file to be uploaded.
  int64 start_range;  // Start of range of contents currently stored in |buf|.
  int64 end_range;  // End of range of contents currently stored in |buf|.
  int64 content_length;  // File content-Length.
  std::string content_type;   // Content-Type of file.
  scoped_refptr<net::IOBuffer> buf;  // Holds current content to be uploaded.
  GURL upload_location;   // Url of where to upload the file to.
  FilePath virtual_path;   // Virtual GData path of the file seen in the UI.
};

// Struct for passing params needed for DocumentsService::InitiateUpload()
// calls.
struct InitiateUploadParams {
  InitiateUploadParams(const std::string& title,
                       const std::string& content_type,
                       int64 content_length,
                       const GURL& resumable_create_media_link,
                       const FilePath& virtual_path);
  ~InitiateUploadParams();

  std::string title;
  std::string content_type;
  int64 content_length;
  GURL resumable_create_media_link;
  const FilePath& virtual_path;
};

// Different callback types for various functionalities in DocumentsService.

// Callback type for authentication related DocumentService calls.
typedef base::Callback<void(GDataErrorCode error,
                            const std::string& token)> AuthStatusCallback;

// Callback type for DocumentServiceInterface::GetDocuments.
// Note: feed_data argument should be passed using base::Passed(&feed_data), not
// feed_data.Pass().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<base::Value> feed_data)> GetDataCallback;

// Callback type for Delete/Move DocumentServiceInterface calls.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& document_url)> EntryActionCallback;

// Callback type for DownloadDocument/DownloadFile DocumentServiceInterface
// calls.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& content_url,
                            const FilePath& temp_file)> DownloadActionCallback;

// Callback type for getting download data from DownloadFile
// DocumentServiceInterface calls.
typedef base::Callback<void(
    GDataErrorCode error,
    scoped_ptr<std::string> download_data)> GetDownloadDataCallback;

// Callback type for DocumentServiceInterface::InitiateUpload.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& upload_url)> InitiateUploadCallback;

// Callback type for DocumentServiceInterface::ResumeUpload.
typedef base::Callback<void(
    const ResumeUploadResponse& response,
    scoped_ptr<gdata::DocumentEntry> new_entry)> ResumeUploadCallback;

// Callback type used to get result of file search.
// If |error| is not PLATFORM_FILE_OK, |entry| is set to NULL.
typedef base::Callback<void(base::PlatformFileError error, GDataEntry* entry)>
    FindEntryCallback;

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARAMS_H_
