// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_AUDIO_VIDEO_CHECKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_AUDIO_VIDEO_CHECKER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/scoped_platform_file_closer.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/public/browser/utility_process_host_client.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"

namespace content {
class UtilityProcessHost;
}

// Uses a utility process to validate a media file.  If the callback returns
// PLATFORM_FILE_OK, then the file appears to be a valid media file. This does
// not attempt to decode the entire file, which may take a considerable amount
// of time.  This class may be constructed on any thread, but should run on the
// IO thread.
class SafeAudioVideoChecker : public content::UtilityProcessHostClient {
 public:
  // Takes responsibility for closing |file|.
  SafeAudioVideoChecker(
      const base::PlatformFile& file,
      const fileapi::CopyOrMoveFileValidator::ResultCallback& callback);

  // Must be called on the IO thread.
  void Start();

 private:
  enum State {
    INITIAL_STATE,
    PINGED_STATE,
    STARTED_STATE,
    FINISHED_STATE
  };

  virtual ~SafeAudioVideoChecker();

  // Starts validation once the utility process has been started.
  virtual void OnProcessStarted();

  // Notification of the result from the utility process.
  void OnCheckingFinished(bool valid);

  // UtilityProcessHostClient implementation.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  State state_;

  // This should be accessed through |file_closer_| to ensure it hasn't already
  // been closed.
  base::PlatformFile file_;

  // Ensures that |file_| is closed if the utility process is not started for
  // some reason.
  base::ScopedPlatformFileCloser file_closer_;

  const fileapi::CopyOrMoveFileValidator::ResultCallback callback_;

  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  DISALLOW_COPY_AND_ASSIGN(SafeAudioVideoChecker);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_AUDIO_VIDEO_CHECKER_H_
