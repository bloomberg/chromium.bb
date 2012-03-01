// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"

#include "base/string_number_conversions.h"
#include "content/public/browser/download_item.h"
#include "net/base/net_util.h"

namespace gdata {

namespace {

const char kUploadContentRange[] = "Content-Range: bytes ";
const char kUploadContentType[] = "X-Upload-Content-Type: ";
const char kUploadContentLength[] = "X-Upload-Content-Length: ";

}  // namespace

UploadFileInfo::UploadFileInfo()
    : file_size(0),
      content_length(0),
      file_stream(NULL),
      buf_len(0),
      start_range(0),
      end_range(-1),
      download_complete(false) {
}

void UploadFileInfo::Init(content::DownloadItem* download) {
  // GetFullPath will be a temporary location if we're streaming.
  file_path = download->GetFullPath();
  file_url = net::FilePathToFileURL(file_path);
  file_size = download->GetReceivedBytes();

  // Use the file name as the title.
  title = download->GetTargetName().BaseName().value();
  content_type = download->GetMimeType();
  // GData api handles -1 as unknown file length.
  content_length = download->IsInProgress() ?
                 -1 : download->GetReceivedBytes();

  download_complete = !download->IsInProgress();
}

UploadFileInfo::~UploadFileInfo() {
}

std::string UploadFileInfo::GetContentRangeHeader() const {
  // The header looks like
  // Content-Range: bytes <start_range>-<end_range>/<content_length>
  // for example:
  // Content-Range: bytes 7864320-8388607/13851821
  // Use * for unknown/streaming content length.
  DCHECK_GE(start_range, 0);
  DCHECK_GE(end_range, 0);
  DCHECK_GE(content_length, -1);
  return std::string(kUploadContentRange) +
         base::Int64ToString(start_range) + "-" +
         base::Int64ToString(end_range) + "/" +
         (content_length == -1 ? "*" : base::Int64ToString(content_length));
}

std::vector<std::string> UploadFileInfo::GetContentTypeAndLengthHeaders()
    const {
  std::vector<std::string> headers;
  headers.push_back(kUploadContentType + content_type);
  headers.push_back(
      kUploadContentLength + base::Int64ToString(content_length));
  return headers;
}

base::StringPiece UploadFileInfo::GetContent() {
  return base::StringPiece(buf->data(), end_range - start_range + 1);
}

}  // namespace gdata
