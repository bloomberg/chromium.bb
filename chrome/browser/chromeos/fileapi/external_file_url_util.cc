// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace chromeos {

GURL FilePathToExternalFileURL(const base::FilePath& path) {
  std::string url(base::StringPrintf(
      "%s:%s", chrome::kExternalFileScheme, path.AsUTF8Unsafe().c_str()));
  return GURL(url);
}

base::FilePath ExternalFileURLToFilePath(const GURL& url) {
  if (!url.is_valid() || url.scheme() != chrome::kExternalFileScheme)
    return base::FilePath();
  std::string path_string =
      net::UnescapeURLComponent(url.GetContent(), net::UnescapeRule::NORMAL);
  return base::FilePath::FromUTF8Unsafe(path_string);
}

GURL CreateExternalFileURLFromPath(Profile* profile,
                                   const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!drive::util::IsUnderDriveMountPoint(path))
    return GURL();

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile);
  if (!file_system)
    return GURL();

  return FilePathToExternalFileURL(drive::util::ExtractDrivePath(path));
}

}  // namespace chromeos
