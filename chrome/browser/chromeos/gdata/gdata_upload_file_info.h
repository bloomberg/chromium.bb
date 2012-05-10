// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOAD_FILE_INFO_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOAD_FILE_INFO_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"

namespace content {
class DownloadItem;
}

namespace gdata {

class DocumentEntry;

// Structure containing current upload information of file, passed between
// DocumentsService methods and callbacks.
struct UploadFileInfo {
  UploadFileInfo();
  ~UploadFileInfo();

  // Bytes left to upload.
  int64 SizeRemaining() const;

  // Useful for printf debugging.
  std::string DebugString() const;

  int upload_id;  // id of this upload.
  FilePath file_path;  // The path of the file to be uploaded.
  int64 file_size;  // Last known size of the file.

  // TODO(zelirag, achuith): Make this string16.
  std::string title;  // Title to be used for file to be uploaded.
  std::string content_type;  // Content-Type of file.
  int64 content_length;  // Header content-Length.

  // Data cached by caller and used when preparing upload data in chunks for
  // multiple ResumeUpload requests.
  // Location URL where file is to be uploaded to, returned from InitiateUpload.
  GURL upload_location;
  // Final path in gdata. Looks like /special/drive/MyFolder/MyFile.
  FilePath gdata_path;

  // TODO(achuith): Use generic stream object after FileStream is refactored to
  // extend a generic stream.
  net::FileStream* file_stream;  // For opening and reading from physical file.
  scoped_refptr<net::IOBuffer> buf;  // Holds current content to be uploaded.
  // Size of |buf|, max is 512KB; Google Docs requires size of each upload chunk
  // to be a multiple of 512KB.
  int64 buf_len;

  // Data to be updated by caller before sending each ResumeUpload request.
  // Note that end_range is inclusive, so start_range=0, end_range=8 is 9 bytes.
  int64 start_range;  // Start of range of contents currently stored in |buf|.
  int64 end_range;  // End of range of contents currently stored in |buf|.

  bool all_bytes_present;  // Whether all bytes of this file are present.
  bool upload_paused;  // Whether this file's upload has been paused.

  bool should_retry_file_open;  // Whether we should retry opening this file.
  int num_file_open_tries;  // Number of times we've tried to open this file.

  // Will be set once the upload is complete.
  scoped_ptr<DocumentEntry> entry;

  // Callback to be invoked once the upload has completed.
  typedef base::Callback<void(base::PlatformFileError error,
      UploadFileInfo* upload_file_info)> UploadCompletionCallback;
  UploadCompletionCallback completion_callback;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOAD_FILE_INFO_H_
