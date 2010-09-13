// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_types.h"

DownloadCreateInfo::DownloadCreateInfo(const FilePath& path,
                                       const GURL& url,
                                       base::Time start_time,
                                       int64 received_bytes,
                                       int64 total_bytes,
                                       int32 state,
                                       int32 download_id)
    : path(path),
      url(url),
      path_uniquifier(0),
      start_time(start_time),
      received_bytes(received_bytes),
      total_bytes(total_bytes),
      state(state),
      download_id(download_id),
      child_id(-1),
      render_view_id(-1),
      request_id(-1),
      db_handle(0),
      prompt_user_for_save_location(false),
      is_dangerous(false),
      is_extension_install(false) {
}

DownloadCreateInfo::DownloadCreateInfo()
    : path_uniquifier(0),
      received_bytes(0),
      total_bytes(0),
      state(-1),
      download_id(-1),
      child_id(-1),
      render_view_id(-1),
      request_id(-1),
      db_handle(0),
      prompt_user_for_save_location(false),
      is_dangerous(false),
      is_extension_install(false) {
}

DownloadCreateInfo::~DownloadCreateInfo() {
}
