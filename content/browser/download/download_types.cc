// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_types.h"

DownloadSaveInfo::DownloadSaveInfo()
    : offset(0) {
}

DownloadSaveInfo::DownloadSaveInfo(const DownloadSaveInfo& info)
    : file_path(info.file_path),
      file_stream(info.file_stream),
      suggested_name(info.suggested_name),
      offset(info.offset),
      hash_state(info.hash_state) {
}

DownloadSaveInfo::~DownloadSaveInfo() {
}

DownloadSaveInfo& DownloadSaveInfo::operator=(const DownloadSaveInfo& info) {
  file_path = info.file_path;
  file_stream = info.file_stream;
  suggested_name = info.suggested_name;
  offset = info.offset;
  suggested_name = info.suggested_name;
  return *this;
}
