// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_PATH_SANITIZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_PATH_SANITIZER_H_

#include "base/files/file_path.h"
#include "base/macros.h"

namespace safe_browsing {

// A helper class to sanitize any number of file paths by replacing the portion
// that represents the current user's home directory with "~".
class PathSanitizer {
 public:
  PathSanitizer();

  const base::FilePath& GetHomeDirectory() const;

  void StripHomeDirectory(base::FilePath* file_path) const;

 private:
  base::FilePath home_path_;

  DISALLOW_COPY_AND_ASSIGN(PathSanitizer);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_PATH_SANITIZER_H_
