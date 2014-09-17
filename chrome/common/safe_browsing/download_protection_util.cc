// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_protection_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace safe_browsing {
namespace download_protection_util {

bool IsArchiveFile(const base::FilePath& file) {
  return file.MatchesExtension(FILE_PATH_LITERAL(".zip"));
}

bool IsBinaryFile(const base::FilePath& file) {
  return (
      // Executable extensions for MS Windows.
      file.MatchesExtension(FILE_PATH_LITERAL(".bas")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".bat")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".cab")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".cmd")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".com")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".exe")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".hta")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".msi")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".pif")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".reg")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".scr")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".vb")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".vbs")) ||
      // Chrome extensions and android APKs are also reported.
      file.MatchesExtension(FILE_PATH_LITERAL(".crx")) ||
      file.MatchesExtension(FILE_PATH_LITERAL(".apk")) ||
      // Archives _may_ contain binaries, we'll check in ExtractFileFeatures.
      IsArchiveFile(file));
}

ClientDownloadRequest::DownloadType GetDownloadType(
    const base::FilePath& file) {
  DCHECK(IsBinaryFile(file));
  if (file.MatchesExtension(FILE_PATH_LITERAL(".apk")))
    return ClientDownloadRequest::ANDROID_APK;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".crx")))
    return ClientDownloadRequest::CHROME_EXTENSION;
  // For zip files, we use the ZIPPED_EXECUTABLE type since we will only send
  // the pingback if we find an executable inside the zip archive.
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".zip")))
    return ClientDownloadRequest::ZIPPED_EXECUTABLE;
  return ClientDownloadRequest::WIN_EXECUTABLE;
}

}  // namespace download_protection_util
}  // namespace safe_browsing
