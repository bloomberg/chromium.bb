// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_
#define IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace reading_list {

// The distilled URL chrome://offline/... that will load the file at |path|.
GURL DistilledURLForPath(const base::FilePath& path);

// The file URL pointing to the local file to load to display |distilled_url|.
// If |resources_root_url| is not nullptr, it is set to a file URL to the
// directory conatining all the resources needed by |distilled_url|.
// |profile_path| is the path to the profile directory.
GURL FileURLForDistilledURL(const GURL& distilled_url,
                            const base::FilePath& profile_path,
                            GURL* resources_root_url);

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_
