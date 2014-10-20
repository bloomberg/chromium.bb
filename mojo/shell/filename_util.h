// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_FILENAME_UTIL_H_
#define MOJO_SHELL_FILENAME_UTIL_H_

class GURL;

namespace base {
class FilePath;
}

namespace mojo {

// Given the full path to a file name, creates a file: URL. The returned URL
// may not be valid if the input is malformed.
GURL FilePathToFileURL(const base::FilePath& path);

}  // namespace mojo

#endif  // MOJO_SHELL_FILENAME_UTIL_H_
