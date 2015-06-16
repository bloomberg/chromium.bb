// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSIONS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSIONS_H_

#include <string>

#include "base/files/file_path.h"

namespace download_util {

enum DownloadDangerLevel {
  // Safe. Or at least not known to be dangerous. Safe to download and open,
  // even if the download was accidental.
  NOT_DANGEROUS,

  // Require confirmation before downloading. An additional user gesture may not
  // be required if the download was from a familiar site and the download was
  // initiated via a user action.
  ALLOW_ON_USER_GESTURE,

  // Always require confirmation when downloading.
  DANGEROUS
};

// Determine the download danger level of a file.
DownloadDangerLevel GetFileDangerLevel(const base::FilePath& path);

// Returns true if the file specified by |path| is allowed to open
// automatically.
//
// Not all downloads are initiated with the consent of the user. Even when the
// user consents, the file written to disk may differ from the users'
// expectations. I.e. a malicious website could drop a nefarious download
// possibly by click jacking, or by serving a file that is different from what
// the user intended to download.
//
// Any prompting done in order to validate a dangerous download is a speed bump
// rather than a security measure. The user likely doesn't have the information
// necessary to evaluate the safety of a downloaded file. In addition, downloads
// with a danger type of ALLOW_ON_USER_GESTURE might not prompt at all. So
// Chrome forces the user to manually open some file types by preventing them
// from being opened automatically. See https://crbug.com/461858
//
// See DownloadAutoOpenHint for details on the criteria used to disallow
// automatic opening for a file type.
bool IsAllowedToOpenAutomatically(const base::FilePath& path);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSIONS_H_
