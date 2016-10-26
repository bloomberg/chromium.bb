// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "net/base/escape.h"

namespace arc {

const char kMountPointName[] = "arc-content";

const base::FilePath::CharType kMountPointPath[] =
    FILE_PATH_LITERAL("/special/arc-content");

GURL ArcUrlToExternalFileUrl(const GURL& arc_url) {
  // Return "externalfile:arc-content/<|arc_url| escaped>".
  base::FilePath virtual_path =
      base::FilePath::FromUTF8Unsafe(kMountPointName)
          .Append(base::FilePath::FromUTF8Unsafe(
              net::EscapeQueryParamValue(arc_url.spec(), false)));
  return chromeos::VirtualPathToExternalFileURL(virtual_path);
}

GURL ExternalFileUrlToArcUrl(const GURL& external_file_url) {
  base::FilePath virtual_path =
      chromeos::ExternalFileURLToVirtualPath(external_file_url);
  base::FilePath path_after_root;
  if (!base::FilePath::FromUTF8Unsafe(kMountPointName)
           .AppendRelativePath(virtual_path, &path_after_root)) {
    return GURL();
  }
  return GURL(net::UnescapeURLComponent(
      path_after_root.AsUTF8Unsafe(),
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
          net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));
}

}  // namespace arc
