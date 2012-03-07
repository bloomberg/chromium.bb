// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"

#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "content/public/browser/download_item.h"
#include "net/base/net_util.h"

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

void UploadFileInfo::Init(content::DownloadItem* download) {
  // GetFullPath will be a temporary location if we're streaming.
  file_path = download->GetFullPath();
  file_url = net::FilePathToFileURL(file_path);
  file_size = download->GetReceivedBytes();

  // Extract the final path from DownloadItem.
  gdata_path = util::GetGDataPath(download);

  // Use the file name as the title.
  title = gdata_path.BaseName().value();
  content_type = download->GetMimeType();
  // GData api handles -1 as unknown file length.
  content_length = download->IsInProgress() ?
                 -1 : download->GetReceivedBytes();

  download_complete = !download->IsInProgress();
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
