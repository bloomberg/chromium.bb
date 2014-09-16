// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_

#include "base/files/file_path.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"

struct DownloadTargetInfo {
  DownloadTargetInfo();
  ~DownloadTargetInfo();

  // Final full target path of the download. Must be non-empty for the remaining
  // fields to be considered valid. The path is a local file system path. Any
  // existing file at this path should be overwritten.
  base::FilePath target_path;

  // Disposition. This will be TARGET_DISPOSITION_PROMPT if the user was
  // prompted during the process of determining the download target. Otherwise
  // it will be TARGET_DISPOSITION_OVERWRITE.
  content::DownloadItem::TargetDisposition target_disposition;

  // Danger type of the download.
  content::DownloadDangerType danger_type;

  // The danger type of the download could be set to MAYBE_DANGEROUS_CONTENT if
  // the file type is handled by SafeBrowsing. However, if the SafeBrowsing
  // service is unable to verify whether the file is safe or not, we are on our
  // own. This flag indicates whether the download should be considered
  // dangerous if SafeBrowsing returns an unknown verdict.
  //
  // Note that some downloads (e.g. "Save link as" on a link to a binary) would
  // not be considered 'Dangerous' even if SafeBrowsing came back with an
  // unknown verdict. So we can't always show a warning when SB fails.
  bool is_dangerous_file;

  // Suggested intermediate path. The downloaded bytes should be written to this
  // path until all the bytes are available and the user has accepted a
  // dangerous download. At that point, the download can be renamed to
  // |target_path|.
  base::FilePath intermediate_path;

  // MIME type based on the file type of the download. This may be different
  // from DownloadItem::GetMimeType() since the latter is based on the server
  // response, and this one is based on the filename.
  std::string mime_type;

  // Whether the |target_path| would be handled safely by the browser if it were
  // to be opened with a file:// URL. This can be used later to decide how file
  // opens should be handled. The file is considered to be handled safely if the
  // filetype is supported by the renderer or a sandboxed plug-in.
  bool is_filetype_handled_safely;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_
