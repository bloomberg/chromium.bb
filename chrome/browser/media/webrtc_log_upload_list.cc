// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_log_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

// static
const char* WebRtcLogUploadList::kWebRtcLogListFilename =
    "webrtc_log_uploads.log";

// static
WebRtcLogUploadList* WebRtcLogUploadList::Create(Delegate* delegate) {
  base::FilePath log_dir_path;
  PathService::Get(chrome::DIR_USER_DATA, &log_dir_path);
  base::FilePath upload_log_path =
      log_dir_path.AppendASCII(kWebRtcLogListFilename);
  return new WebRtcLogUploadList(delegate, upload_log_path);
}

WebRtcLogUploadList::WebRtcLogUploadList(Delegate* delegate,
                                         const base::FilePath& upload_log_path)
    : UploadList(delegate, upload_log_path) {}

WebRtcLogUploadList::~WebRtcLogUploadList() {}
