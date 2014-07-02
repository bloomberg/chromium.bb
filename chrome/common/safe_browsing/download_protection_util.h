// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_DOWNLOAD_PROTECTION_UTIL_H_
#define CHROME_COMMON_SAFE_BROWSING_DOWNLOAD_PROTECTION_UTIL_H_

#include "chrome/common/safe_browsing/csd.pb.h"

namespace base {
class FilePath;
}

namespace safe_browsing {
namespace download_protection_util {

// Returns true if the given file is a supported binary file type.
bool IsBinaryFile(const base::FilePath& file);

// Returns true if the given file is a supported archive file type.
bool IsArchiveFile(const base::FilePath& file);

// Returns the DownloadType of the file at |path|. This function is only valid
// for paths that satisfy IsBinaryFile() above.
ClientDownloadRequest::DownloadType GetDownloadType(const base::FilePath& file);

}  // namespace download_protection_util
}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_DOWNLOAD_PROTECTION_UTIL_H_
