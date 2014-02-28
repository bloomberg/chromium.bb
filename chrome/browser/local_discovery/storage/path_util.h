// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PATH_UTIL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PATH_UTIL_H_

#include "base/files/file_path.h"

namespace local_discovery {

base::FilePath NormalizeFilePath(const base::FilePath& path);

struct ParsedPrivetPath {
  explicit ParsedPrivetPath(const base::FilePath& path);
  ~ParsedPrivetPath();

  std::string service_name;
  std::string path;
};


}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PATH_UTIL_H_
