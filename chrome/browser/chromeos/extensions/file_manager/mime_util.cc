// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/mime_util.h"

#include <utility>
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "net/base/mime_util.h"

namespace file_manager {
namespace util {

std::string GetMimeTypeForPath(const base::FilePath& file_path) {
  const base::FilePath::StringType file_extension =
      StringToLowerASCII(file_path.Extension());

  // TODO(thorogood): Rearchitect this call so it can run on the File thread;
  // GetMimeTypeFromFile requires this on Linux. Right now, we use
  // Chrome-level knowledge only.
  std::string mime_type;
  if (file_extension.empty() ||
      !net::GetWellKnownMimeTypeFromExtension(file_extension.substr(1),
                                              &mime_type)) {
    // If the file doesn't have an extension or its mime-type cannot be
    // determined, then indicate that it has the empty mime-type. This will
    // only be matched if the Web Intents accepts "*" or "*/*".
    return "";
  } else {
    return mime_type;
  }
}

void GetMimeTypesForPaths(
    const std::vector<base::FilePath>& file_paths,
    extensions::app_file_handler_util::PathAndMimeTypeSet* path_mime_set) {
  for (std::vector<base::FilePath>::const_iterator iter = file_paths.begin();
       iter != file_paths.end(); ++iter) {
    path_mime_set->insert(
        std::make_pair(*iter, util::GetMimeTypeForPath(*iter)));
  }
}

}  // namespace util
}  // namespace file_manager
