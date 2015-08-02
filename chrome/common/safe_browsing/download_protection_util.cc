// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_protection_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace safe_browsing {
namespace download_protection_util {

bool IsArchiveFile(const base::FilePath& file) {
  // List of interesting archive file formats. These are by no means exhaustive,
  // but are currently file types that Safe Browsing would like to see pings for
  // due to the possibility of them being used as wrapper formats for malicious
  // payloads.
  const base::FilePath::CharType* kArchiveFileTypes[] = {
    FILE_PATH_LITERAL(".zip"),
    FILE_PATH_LITERAL(".rar"),
    FILE_PATH_LITERAL(".7z"),
    FILE_PATH_LITERAL(".cab"),
    FILE_PATH_LITERAL(".xz"),
    FILE_PATH_LITERAL(".gz"),
    FILE_PATH_LITERAL(".tgz"),
    FILE_PATH_LITERAL(".bz2"),
    FILE_PATH_LITERAL(".tar"),
    FILE_PATH_LITERAL(".arj"),
    FILE_PATH_LITERAL(".lzh"),
    FILE_PATH_LITERAL(".lha"),
    FILE_PATH_LITERAL(".wim"),
    FILE_PATH_LITERAL(".z"),
    FILE_PATH_LITERAL(".lzma"),
    FILE_PATH_LITERAL(".cpio"),
  };
  for (const auto& extension : kArchiveFileTypes) {
    if (file.MatchesExtension(extension))
      return true;
  }
  // TODO(mattm): should .dmg be checked here instead of IsSupportedBinaryFile?
  return false;
}

bool IsSupportedBinaryFile(const base::FilePath& file) {
  const base::FilePath::CharType* kSupportedBinaryFileTypes[] = {
      // Executable extensions for MS Windows.
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

  // .zip files are examined for any executables or other archives they may
  // contain. Currently no other archive formats are supported.
  return file.MatchesExtension(FILE_PATH_LITERAL(".zip"));
}

ClientDownloadRequest::DownloadType GetDownloadType(
    const base::FilePath& file) {
  DCHECK(IsSupportedBinaryFile(file));
  if (file.MatchesExtension(FILE_PATH_LITERAL(".apk")))
    return ClientDownloadRequest::ANDROID_APK;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".crx")))
    return ClientDownloadRequest::CHROME_EXTENSION;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".zip")))
    // DownloadProtectionService doesn't send a ClientDownloadRequest for ZIP
    // files unless they contain either executables or archives. The resulting
    // DownloadType is either ZIPPED_EXECUTABLE or ZIPPED_ARCHIVE respectively.
    // This function will return ZIPPED_EXECUTABLE for ZIP files as a
    // placeholder. The correct DownloadType will be determined based on the
    // result of analyzing the ZIP file.
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
