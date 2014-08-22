// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_AV_SCANNING_FILE_VALIDATOR_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_AV_SCANNING_FILE_VALIDATOR_H_

#include "base/basictypes.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"

namespace base {
class FilePath;
}  // namespace base

// This class supports AV scanning on post write validation.
class AVScanningFileValidator : public storage::CopyOrMoveFileValidator {
 public:
  virtual ~AVScanningFileValidator();

  // Runs AV checks on the resulting file (Windows-only).
  // Subclasses will not typically override this method.
  virtual void StartPostWriteValidation(
      const base::FilePath& dest_platform_path,
      const ResultCallback& result_callback) OVERRIDE;

 protected:
  AVScanningFileValidator();

 private:
  DISALLOW_COPY_AND_ASSIGN(AVScanningFileValidator);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_AV_SCANNING_FILE_VALIDATOR_H_
