// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UTIL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UTIL_H_

#include "base/files/file_path.h"
#include "base/time/time.h"

class WebRtcLogUtil {
 public:
  // Deletes logs files older that 5 days. Updates the log file list. Must be
  // called on the FILE thread.
  static void DeleteOldWebRtcLogFiles(const base::FilePath& log_dir);

  // Deletes logs files older that 5 days and logs younger than
  // |delete_begin_time|. Updates the log file list. If |delete_begin_time| is
  // base::time::Max(), no recent logs will be deleted, and the function is
  // equal to DeleteOldWebRtcLogFiles(). Must be called on the FILE thread.
  static void DeleteOldAndRecentWebRtcLogFiles(
      const base::FilePath& log_dir,
      const base::Time& delete_begin_time);

  // Calls DeleteOldWebRtcLogFiles() for all profiles. Must be called on the UI
  // thread.
  static void DeleteOldWebRtcLogFilesForAllProfiles();
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UTIL_H_
