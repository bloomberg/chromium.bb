// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_OFFLINE_URL_UTILS_H_
#define COMPONENTS_READING_LIST_OFFLINE_URL_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace reading_list {

// The absolute path of the directory where offline URLs are saved.
// |profile_path| is the path to the profile directory that contain the offline
// directory.
base::FilePath OfflineRootDirectoryPath(const base::FilePath& profile_path);

// The absolute path of the directory where a |url| is saved offline.
// Contains the page and supporting files (images).
// |profile_path| is the path to the profile directory that contain the offline
// directory.
// The directory may not exist.
base::FilePath OfflineURLDirectoryAbsolutePath(
    const base::FilePath& profile_path,
    const GURL& url);

// The absolute path of the offline webpage for the |url|. The file may not
// exist.
// |profile_path| is the path to the profile directory that contain the offline
// directory.
base::FilePath OfflinePageAbsolutePath(const base::FilePath& profile_path,
                                       const GURL& url);

// The relative path to the offline webpage for the |url|. The result is
// relative to |OfflineRootDirectoryPath()|.
// The file may not exist.
base::FilePath OfflinePagePath(const GURL& url);

// The name of the directory containing offline data for |url|.
std::string OfflineURLDirectoryID(const GURL& url);

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_OFFLINE_URL_UTILS_H_
