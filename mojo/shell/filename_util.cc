// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/filename_util.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/url_canon_internal.h"
#include "url/url_util.h"

namespace mojo {

// Prefix to prepend to get a file URL.
static const base::FilePath::CharType kFileURLPrefix[] =
    FILE_PATH_LITERAL("file://");

GURL FilePathToFileURL(const base::FilePath& path) {
  // Produce a URL like "file:///C:/foo" for a regular file, or
  // "file://///server/path" for UNC. The URL canonicalizer will fix up the
  // latter case to be the canonical UNC form: "file://server/path"
  base::FilePath::StringType url_string(kFileURLPrefix);
  if (!path.IsAbsolute()) {
    base::FilePath current_dir;
    PathService::Get(base::DIR_CURRENT, &current_dir);
    url_string.append(current_dir.value());
    url_string.push_back(base::FilePath::kSeparators[0]);
  }
  url_string.append(path.value());

  // Now do replacement of some characters. Since we assume the input is a
  // literal filename, anything the URL parser might consider special should
  // be escaped here.

  // This must be the first substitution since others will introduce percents as
  // the escape character
  ReplaceSubstringsAfterOffset(
      &url_string, 0, FILE_PATH_LITERAL("%"), FILE_PATH_LITERAL("%25"));

  // A semicolon is supposed to be some kind of separator according to RFC 2396.
  ReplaceSubstringsAfterOffset(
      &url_string, 0, FILE_PATH_LITERAL(";"), FILE_PATH_LITERAL("%3B"));

  ReplaceSubstringsAfterOffset(
      &url_string, 0, FILE_PATH_LITERAL("#"), FILE_PATH_LITERAL("%23"));

  ReplaceSubstringsAfterOffset(
      &url_string, 0, FILE_PATH_LITERAL("?"), FILE_PATH_LITERAL("%3F"));

#if defined(OS_POSIX)
  ReplaceSubstringsAfterOffset(
      &url_string, 0, FILE_PATH_LITERAL("\\"), FILE_PATH_LITERAL("%5C"));
#endif

  return GURL(url_string);
}

}  // namespace mojo
