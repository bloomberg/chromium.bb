// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_LIST_H_

#include "components/upload_list/upload_list.h"

class Profile;

class WebRtcLogList {
 public:
  // Creates the upload list with the given callback delegate for a
  // profile. The upload list loads and parses a text file list of WebRTC
  // logs stored locally and/or uploaded.
  static UploadList* CreateWebRtcLogList(UploadList::Delegate* delegate,
                                         Profile* profile);

  // Get the file path for the log directory for a profile.
  static base::FilePath GetWebRtcLogDirectoryForProfile(
      const base::FilePath& profile_path);

  // Get the file path for the log list file in a directory. The log list file
  // name will be appended to |dir| and returned.
  static base::FilePath GetWebRtcLogListFileForDirectory(
      const base::FilePath& dir);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_LIST_H_
