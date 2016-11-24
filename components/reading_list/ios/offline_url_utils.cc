// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/offline_url_utils.h"

#include "base/md5.h"
#include "base/strings/stringprintf.h"

namespace {
const char kOfflineDirectory[] = "Offline";
const char kMainPageFileName[] = "page.html";
}  // namespace

namespace reading_list {

base::FilePath OfflineRootDirectoryPath(const base::FilePath& profile_path) {
  return profile_path.Append(FILE_PATH_LITERAL(kOfflineDirectory));
}

std::string OfflineURLDirectoryID(const GURL& url) {
  return base::MD5String(url.spec());
}

base::FilePath OfflinePagePath(const GURL& url) {
  base::FilePath directory(OfflineURLDirectoryID(url));
  return directory.Append(FILE_PATH_LITERAL(kMainPageFileName));
}

base::FilePath OfflineURLDirectoryAbsolutePath(
    const base::FilePath& profile_path,
    const GURL& url) {
  return OfflineRootDirectoryPath(profile_path)
      .Append(OfflineURLDirectoryID(url));
}

base::FilePath OfflinePageAbsolutePath(const base::FilePath& profile_path,
                                       const GURL& url) {
  return OfflineRootDirectoryPath(profile_path).Append(OfflinePagePath(url));
}
}
