// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/offline_url_utils.h"

#include "base/md5.h"
#include "base/strings/stringprintf.h"
#include "ios/chrome/browser/chrome_url_constants.h"

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

GURL DistilledURLForPath(const base::FilePath& distilled_path) {
  if (distilled_path.empty()) {
    return GURL();
  }
  return GURL(kChromeUIOfflineURL + distilled_path.value());
}

GURL FileURLForDistilledURL(const GURL& distilled_url,
                            const base::FilePath& profile_path,
                            GURL* resources_root_url) {
  if (!distilled_url.is_valid()) {
    return GURL();
  }
  DCHECK(distilled_url.SchemeIs(kChromeUIScheme));
  base::FilePath offline_path = profile_path.AppendASCII(kOfflineDirectory);

  GURL file_url(base::StringPrintf("%s%s", url::kFileScheme,
                                   url::kStandardSchemeSeparator) +
                offline_path.value() + distilled_url.path());
  if (resources_root_url) {
    *resources_root_url = file_url.Resolve(".");
  }
  return file_url;
}
}
