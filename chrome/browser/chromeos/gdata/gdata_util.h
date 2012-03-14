// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#pragma once

#include <string>

class FilePath;

namespace content {
class DownloadItem;
class DownloadManager;
}

namespace gdata {
namespace util {

// Returns the GData mount point path, which looks like "/special/gdata".
const FilePath& GetGDataMountPointPath();

// Returns the GData mount path as string.
const std::string& GetGDataMountPointPathAsString();

// Returns true if the given path is under the GData mount point.
bool IsUnderGDataMountPoint(const FilePath& path);

// Extracts the GData path from the given path located under the GData mount
// point. Returns an empty path if |path| is not under the GData mount point.
// Examples: ExtractGDatPath("/special/gdata/foo.txt") => "gdata/foo.txt"
FilePath ExtractGDataPath(const FilePath& path);

// Files to be uploaded to GData are downloaded to this temporary folder first,
// located at ~/Downloads/.gdata
FilePath GetGDataTempDownloadFolderPath();

// Parses the xml response of HTTP_CREATED response to extract the
// resourceId and md5Checksum.
void ParseCreatedResponseContent(const std::string& response_content,
                                 std::string* resource_id,
                                 std::string* md5_checksum);

}  // namespace util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
