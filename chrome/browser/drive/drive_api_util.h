// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_
#define CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_

#include <string>

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"

class GURL;

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace google_apis {
class ChangeList;
class ChangeResource;
class FileList;
class FileResource;
class ResourceEntry;
}  // namespace google_apis

namespace net {
class IOBuffer;
}  // namespace net

namespace storage {
class FileStreamReader;
}  // namespace storage

namespace drive {
namespace util {

// Google Apps MIME types:
const char kGoogleDocumentMimeType[] = "application/vnd.google-apps.document";
const char kGoogleDrawingMimeType[] = "application/vnd.google-apps.drawing";
const char kGooglePresentationMimeType[] =
    "application/vnd.google-apps.presentation";
const char kGoogleSpreadsheetMimeType[] =
    "application/vnd.google-apps.spreadsheet";
const char kGoogleTableMimeType[] = "application/vnd.google-apps.table";
const char kGoogleFormMimeType[] = "application/vnd.google-apps.form";
const char kGoogleMapMimeType[] = "application/vnd.google-apps.map";
const char kDriveFolderMimeType[] = "application/vnd.google-apps.folder";

// Escapes ' to \' in the |str|. This is designed to use for string value of
// search parameter on Drive API v2.
// See also: https://developers.google.com/drive/search-parameters
std::string EscapeQueryStringValue(const std::string& str);

// Parses the query, and builds a search query for Drive API v2.
// This only supports:
//   Regular query (e.g. dog => fullText contains 'dog')
//   Conjunctions
//     (e.g. dog cat => fullText contains 'dog' and fullText contains 'cat')
//   Exclusion query (e.g. -cat => not fullText contains 'cat').
//   Quoted query (e.g. "dog cat" => fullText contains 'dog cat').
// See also: https://developers.google.com/drive/search-parameters
std::string TranslateQuery(const std::string& original_query);

// If |resource_id| is in the old resource ID format used by WAPI, converts it
// into the new format.
std::string CanonicalizeResourceId(const std::string& resource_id);

// Returns the (base-16 encoded) MD5 digest of the file content at |file_path|,
// or an empty string if an error is found.
std::string GetMd5Digest(const base::FilePath& file_path);

// Computes the (base-16 encoded) MD5 digest of data extracted from a file
// stream.
class FileStreamMd5Digester {
 public:
  typedef base::Callback<void(const std::string&)> ResultCallback;

  FileStreamMd5Digester();
  ~FileStreamMd5Digester();

  // Computes an MD5 digest of data read from the given |streamReader|.  The
  // work occurs asynchronously, and the resulting hash is returned via the
  // |callback|.  If an error occurs, |callback| is called with an empty string.
  // Only one stream can be processed at a time by each digester.  Do not call
  // GetMd5Digest before the results of a previous call have been returned.
  void GetMd5Digest(scoped_ptr<storage::FileStreamReader> stream_reader,
                    const ResultCallback& callback);

 private:
  // Kicks off a read of the next chunk from the stream.
  void ReadNextChunk(const ResultCallback& callback);
  // Handles the incoming chunk of data from a stream read.
  void OnChunkRead(const ResultCallback& callback, int bytes_read);

  // Maximum chunk size for read operations.
  scoped_ptr<storage::FileStreamReader> reader_;
  scoped_refptr<net::IOBuffer> buffer_;
  base::MD5Context md5_context_;

  DISALLOW_COPY_AND_ASSIGN(FileStreamMd5Digester);
};

// Returns preferred file extension for hosted documents which have given mime
// type.
std::string GetHostedDocumentExtension(const std::string& mime_type);

// Returns true if the given mime type is corresponding to one of known hosted
// document types.
bool IsKnownHostedDocumentMimeType(const std::string& mime_type);

// Returns true if the given file path has an extension corresponding to one of
// hosted document types.
bool HasHostedDocumentExtension(const base::FilePath& path);

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_
