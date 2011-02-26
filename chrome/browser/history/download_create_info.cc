// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_create_info.h"

#include <string>

#include "base/format_macros.h"
#include "base/stringprintf.h"

DownloadCreateInfo::DownloadCreateInfo(const FilePath& path,
                                       const GURL& url,
                                       base::Time start_time,
                                       int64 received_bytes,
                                       int64 total_bytes,
                                       int32 state,
                                       int32 download_id,
                                       bool has_user_gesture)
    : path(path),
      url(url),
      path_uniquifier(0),
      start_time(start_time),
      received_bytes(received_bytes),
      total_bytes(total_bytes),
      state(state),
      download_id(download_id),
      has_user_gesture(has_user_gesture),
      child_id(-1),
      render_view_id(-1),
      request_id(-1),
      db_handle(0),
      prompt_user_for_save_location(false),
      is_dangerous_file(false),
      is_dangerous_url(false),
      is_extension_install(false) {
}

DownloadCreateInfo::DownloadCreateInfo()
    : path_uniquifier(0),
      received_bytes(0),
      total_bytes(0),
      state(-1),
      download_id(-1),
      has_user_gesture(false),
      child_id(-1),
      render_view_id(-1),
      request_id(-1),
      db_handle(0),
      prompt_user_for_save_location(false),
      is_dangerous_file(false),
      is_dangerous_url(false),
      is_extension_install(false) {
}

DownloadCreateInfo::~DownloadCreateInfo() {
}

bool DownloadCreateInfo::IsDangerous() {
  return is_dangerous_url || is_dangerous_file;
}
std::string DownloadCreateInfo::DebugString() const {
  return base::StringPrintf("{"
                            " url_ = \"%s\""
                            " path = \"%" PRFilePath "\""
                            " received_bytes = %" PRId64
                            " total_bytes = %" PRId64
                            " child_id = %d"
                            " render_view_id = %d"
                            " request_id = %d"
                            " download_id = %d"
                            " prompt_user_for_save_location = %c"
                            " }",
                            url.spec().c_str(),
                            path.value().c_str(),
                            received_bytes,
                            total_bytes,
                            child_id,
                            render_view_id,
                            request_id,
                            download_id,
                            prompt_user_for_save_location ? 'T' : 'F');
}
