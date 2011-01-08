// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSIONS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSIONS_H_
#pragma once

#include <string>

#include "base/file_path.h"

namespace download_util {

enum DownloadDangerLevel {
  NotDangerous,
  AllowOnUserGesture,
  Dangerous
};

// Determine the download danger level of a file.
DownloadDangerLevel GetFileDangerLevel(const FilePath& path);

// Determine the download danger level using a file extension.
DownloadDangerLevel GetFileExtensionDangerLevel(
    const FilePath::StringType& extension);

// True if the download danger level of the file is NotDangerous.
bool IsFileSafe(const FilePath& path);

// Tests if we think the server means for this mime_type to be executable.
bool IsExecutableMimeType(const std::string& mime_type);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSIONS_H_
