// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_LOG_UPLOAD_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_LOG_UPLOAD_LIST_H_

#include "chrome/browser/upload_list.h"

// Loads and parses a text file list of uploaded WebRTC logs.
class WebRtcLogUploadList : public UploadList {
 public:
  // Creates the WebRTC log upload list with the given callback delegate.
  static WebRtcLogUploadList* Create(Delegate* delegate);

  // Used in this class when reading the list file and in WebRtcLogUploader when
  // writing to the list file.
  static const char* kWebRtcLogListFilename;

  // Creates a new WebRTC log upload list with the given callback delegate.
  // |upload_log_path| is the full path to the file to read the list from.
  explicit WebRtcLogUploadList(Delegate* delegate,
                               const base::FilePath& upload_log_path);

 protected:
  virtual ~WebRtcLogUploadList();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcLogUploadList);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_LOG_UPLOAD_LIST_H_
