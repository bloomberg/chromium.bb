// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace extensions {
namespace file_util {

base::FilePath ExtensionURLToRelativeFilePath(const GURL& url) {
  std::string url_path = url.path();
  if (url_path.empty() || url_path[0] != '/')
    return base::FilePath();

  // Drop the leading slashes and convert %-encoded UTF8 to regular UTF8.
  std::string file_path = net::UnescapeURLComponent(url_path,
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  size_t skip = file_path.find_first_not_of("/\\");
  if (skip != file_path.npos)
    file_path = file_path.substr(skip);

  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_path);

  // It's still possible for someone to construct an annoying URL whose path
  // would still wind up not being considered relative at this point.
  // For example: chrome-extension://id/c:////foo.html
  if (path.IsAbsolute())
    return base::FilePath();

  return path;
}

base::FilePath ExtensionResourceURLToFilePath(const GURL& url,
                                              const base::FilePath& root) {
  std::string host = net::UnescapeURLComponent(url.host(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  if (host.empty())
    return base::FilePath();

  base::FilePath relative_path = ExtensionURLToRelativeFilePath(url);
  if (relative_path.empty())
    return base::FilePath();

  base::FilePath path = root.AppendASCII(host).Append(relative_path);
  if (!base::PathExists(path))
    return base::FilePath();
  path = base::MakeAbsoluteFilePath(path);
  if (path.empty() || !root.IsParent(path))
    return base::FilePath();
  return path;
}

}  // namespace file_util
}  // namespace extensions
