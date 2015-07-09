// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_protection_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace safe_browsing {
namespace download_protection_util {

bool IsArchiveFile(const base::FilePath& file) {
  // TODO(mattm): should .dmg be checked here instead of IsBinaryFile?
  return file.MatchesExtension(FILE_PATH_LITERAL(".zip"));
}

bool IsBinaryFile(const base::FilePath& file) {
  const base::FilePath::CharType* kSupportedBinaryFileTypes[] = {
      // Executable extensions for MS Windows.
      FILE_PATH_LITERAL(".cab"),
      FILE_PATH_LITERAL(".cmd"),
      FILE_PATH_LITERAL(".com"),
      FILE_PATH_LITERAL(".dll"),
      FILE_PATH_LITERAL(".exe"),
      FILE_PATH_LITERAL(".msc"),
      FILE_PATH_LITERAL(".msi"),
      FILE_PATH_LITERAL(".msp"),
      FILE_PATH_LITERAL(".mst"),
      FILE_PATH_LITERAL(".pif"),
      FILE_PATH_LITERAL(".scr"),
      // Not binary, but still contain executable code, or can be used to launch
      // other executables.
      FILE_PATH_LITERAL(".bas"),
      FILE_PATH_LITERAL(".bat"),
      FILE_PATH_LITERAL(".hta"),
      FILE_PATH_LITERAL(".js"),
      FILE_PATH_LITERAL(".jse"),
      FILE_PATH_LITERAL(".mht"),
      FILE_PATH_LITERAL(".mhtml"),
      FILE_PATH_LITERAL(".msh"),
      FILE_PATH_LITERAL(".msh1"),
      FILE_PATH_LITERAL(".msh1xml"),
      FILE_PATH_LITERAL(".msh2"),
      FILE_PATH_LITERAL(".msh2xml"),
      FILE_PATH_LITERAL(".mshxml"),
      FILE_PATH_LITERAL(".ps1"),
      FILE_PATH_LITERAL(".ps1xml"),
      FILE_PATH_LITERAL(".ps2"),
      FILE_PATH_LITERAL(".ps2xml"),
      FILE_PATH_LITERAL(".psc1"),
      FILE_PATH_LITERAL(".psc2"),
      FILE_PATH_LITERAL(".reg"),
      FILE_PATH_LITERAL(".scf"),
      FILE_PATH_LITERAL(".sct"),
      FILE_PATH_LITERAL(".url"),
      FILE_PATH_LITERAL(".vb"),
      FILE_PATH_LITERAL(".vbe"),
      FILE_PATH_LITERAL(".vbs"),
      FILE_PATH_LITERAL(".website"),
      FILE_PATH_LITERAL(".wsf"),
      // Chrome extensions and android APKs are also reported.
      FILE_PATH_LITERAL(".apk"),
      FILE_PATH_LITERAL(".crx"),
      // Mac extensions.
      FILE_PATH_LITERAL(".app"),
      FILE_PATH_LITERAL(".dmg"),
      FILE_PATH_LITERAL(".osx"),
      FILE_PATH_LITERAL(".pkg"),
  };
  for (const auto& extension : kSupportedBinaryFileTypes)
    if (file.MatchesExtension(extension))
      return true;

  // Archives _may_ contain binaries, we'll check in ExtractFileFeatures.
  return IsArchiveFile(file);
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
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".dmg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pkg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".osx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".app")))
    return ClientDownloadRequest::MAC_EXECUTABLE;
  return ClientDownloadRequest::WIN_EXECUTABLE;
}

}  // namespace download_protection_util
}  // namespace safe_browsing
