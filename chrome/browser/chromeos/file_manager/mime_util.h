// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides MIME related utilities.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MIME_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MIME_UTIL_H_

#include <string>

#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace file_manager {
namespace util {

// Returns the MIME type of |file_path|. Returns "" if the MIME type is
// unknown.
std::string GetMimeTypeForPath(const base::FilePath& file_path);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MIME_UTIL_H_
