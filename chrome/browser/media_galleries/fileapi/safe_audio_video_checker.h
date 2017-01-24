// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_AUDIO_VIDEO_CHECKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_AUDIO_VIDEO_CHECKER_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/media_parser.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "storage/browser/fileapi/copy_or_move_file_validator.h"

// Uses a utility process to validate a media file.  If the callback returns
// File::FILE_OK, then file appears to be valid.  File validation does not
// attempt to decode the entire file since that could take a considerable
// amount of time.  This class can be constructed on any thread, but should
// run on the IO thread.
class SafeAudioVideoChecker
    : public base::RefCountedThreadSafe<SafeAudioVideoChecker> {
 public:
  // Takes responsibility for closing |file|.
  SafeAudioVideoChecker(
      base::File file,
      const storage::CopyOrMoveFileValidator::ResultCallback& callback);

  // Check the file: must be called on the IO thread.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<SafeAudioVideoChecker>;

  ~SafeAudioVideoChecker();

  // Media file check result.
  void CheckMediaFileDone(bool valid);

  // Media file to check.
  base::File file_;

  // Utility process used to check |file_|.
  std::unique_ptr<
      content::UtilityProcessMojoClient<extensions::mojom::MediaParser>>
      utility_process_mojo_client_;

  // Report the check result to |callback_|.
  const storage::CopyOrMoveFileValidator::ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeAudioVideoChecker);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_AUDIO_VIDEO_CHECKER_H_
