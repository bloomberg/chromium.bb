// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/util/filename_util.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"
#include "url/url_canon_internal.h"
#include "url/url_util.h"

namespace mojo {
namespace util {

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
  base::ReplaceSubstringsAfterOffset(&url_string, 0, FILE_PATH_LITERAL("%"),
                                     FILE_PATH_LITERAL("%25"));

  // A semicolon is supposed to be some kind of separator according to RFC 2396.
  base::ReplaceSubstringsAfterOffset(&url_string, 0, FILE_PATH_LITERAL(";"),
                                     FILE_PATH_LITERAL("%3B"));

  base::ReplaceSubstringsAfterOffset(&url_string, 0, FILE_PATH_LITERAL("#"),
                                     FILE_PATH_LITERAL("%23"));

  base::ReplaceSubstringsAfterOffset(&url_string, 0, FILE_PATH_LITERAL("?"),
                                     FILE_PATH_LITERAL("%3F"));

#if defined(OS_POSIX)
  base::ReplaceSubstringsAfterOffset(&url_string, 0, FILE_PATH_LITERAL("\\"),
                                     FILE_PATH_LITERAL("%5C"));
#endif

  return GURL(url_string);
}

GURL AddTrailingSlashIfNeeded(const GURL& url) {
  if (!url.has_path() || *url.path().rbegin() == '/')
    return url;

  std::string path(url.path() + '/');
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

base::FilePath UrlToFilePath(const GURL& url) {
  DCHECK(url.SchemeIsFile());
  url::RawCanonOutputW<1024> output;
  url::DecodeURLEscapeSequences(url.path().data(),
                                static_cast<int>(url.path().length()), &output);
  base::string16 decoded_path = base::string16(output.data(), output.length());
#if defined(OS_WIN)
  base::TrimString(decoded_path, L"/", &decoded_path);
  base::FilePath path(decoded_path);
#else
  base::FilePath path(base::UTF16ToUTF8(decoded_path));
#endif
  return path;
}

}  // namespace util
}  // namespace mojo
