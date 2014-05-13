// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_validator_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/media_galleries/fileapi/supported_audio_video_checker.h"
#include "chrome/browser/media_galleries/fileapi/supported_image_type_validator.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace {

class InvalidFileValidator : public fileapi::CopyOrMoveFileValidator {
 public:
  virtual ~InvalidFileValidator() {}
  virtual void StartPreWriteValidation(
      const fileapi::CopyOrMoveFileValidator::ResultCallback&
          result_callback) OVERRIDE {
    result_callback.Run(base::File::FILE_ERROR_SECURITY);
  }

  virtual void StartPostWriteValidation(
      const base::FilePath& dest_platform_path,
      const fileapi::CopyOrMoveFileValidator::ResultCallback&
          result_callback) OVERRIDE {
    result_callback.Run(base::File::FILE_ERROR_SECURITY);
  }

 private:
  friend class ::MediaFileValidatorFactory;

  InvalidFileValidator() {}

  DISALLOW_COPY_AND_ASSIGN(InvalidFileValidator);
};

}  // namespace

MediaFileValidatorFactory::MediaFileValidatorFactory() {}
MediaFileValidatorFactory::~MediaFileValidatorFactory() {}

fileapi::CopyOrMoveFileValidator*
MediaFileValidatorFactory::CreateCopyOrMoveFileValidator(
    const fileapi::FileSystemURL& src,
    const base::FilePath& platform_path) {
  base::FilePath src_path = src.virtual_path();
  if (SupportedImageTypeValidator::SupportsFileType(src_path))
    return new SupportedImageTypeValidator(platform_path);
  if (SupportedAudioVideoChecker::SupportsFileType(src_path))
    return new SupportedAudioVideoChecker(platform_path);

  return new InvalidFileValidator();
}
