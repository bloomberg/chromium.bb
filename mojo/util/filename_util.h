// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_UTIL_FILENAME_UTIL_H_
#define MOJO_UTIL_FILENAME_UTIL_H_

class GURL;

namespace base {
class FilePath;
}

namespace mojo {
namespace util {

// Given the full path to a file name, creates a file: URL. The returned URL
// may not be valid if the input is malformed.
GURL FilePathToFileURL(const base::FilePath& path);

// This URL is going to be treated as a directory. Ensure there is a trailing
// slash so that GURL.Resolve(...) works correctly.
GURL AddTrailingSlashIfNeeded(const GURL& url);

// Converts a file url to a path
base::FilePath UrlToFilePath(const GURL& url);

}  // namespace util
}  // namespace mojo

#endif  // MOJO_UTIL_FILENAME_UTIL_H_
