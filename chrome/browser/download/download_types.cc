// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_types.h"

DownloadBuffer::DownloadBuffer() {
}

DownloadBuffer::~DownloadBuffer() {
}

DownloadSaveInfo::DownloadSaveInfo() {
}

DownloadSaveInfo::DownloadSaveInfo(const DownloadSaveInfo& info)
    : file_path(info.file_path),
      file_stream(info.file_stream) {
}

DownloadSaveInfo::~DownloadSaveInfo() {
}

DownloadSaveInfo& DownloadSaveInfo::operator=(const DownloadSaveInfo& info) {
  file_path = info.file_path;
  file_stream = info.file_stream;
  return *this;
}
