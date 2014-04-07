// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_AUDIO_VIDEO_CHECKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_AUDIO_VIDEO_CHECKER_H_

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"

class MediaFileValidatorFactory;
class SafeAudioVideoChecker;

// Uses SafeAudioVideoChecker to validate supported audio and video files in
// the utility process and then uses AVScanningFileValidator to ask the OS to
// virus scan them. The entire file is not decoded so a positive result from
// this class does not make the file safe to use in the browser process.
class SupportedAudioVideoChecker : public AVScanningFileValidator {
 public:
  virtual ~SupportedAudioVideoChecker();

  static bool SupportsFileType(const base::FilePath& path);

  virtual void StartPreWriteValidation(
      const ResultCallback& result_callback) OVERRIDE;

 private:
  friend class MediaFileValidatorFactory;

  explicit SupportedAudioVideoChecker(const base::FilePath& file);

  void OnFileOpen(base::File file);

  base::FilePath path_;
  fileapi::CopyOrMoveFileValidator::ResultCallback callback_;
  scoped_refptr<SafeAudioVideoChecker> safe_checker_;
  base::WeakPtrFactory<SupportedAudioVideoChecker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupportedAudioVideoChecker);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SUPPORTED_AUDIO_VIDEO_CHECKER_H_
