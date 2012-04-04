// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"

class FilePath;
class Profile;

namespace gdata {
namespace util {

// Returns the GData mount point path, which looks like "/special/gdata".
const FilePath& GetGDataMountPointPath();

// Returns the GData mount path as string.
const std::string& GetGDataMountPointPathAsString();

// Returns the 'local' root of remote file system as "/special".
const FilePath& GetSpecialRemoteRootPath();

// Returns the gdata file resource url formatted as
// chrome://gdata/<resource_id>/<file_name>.
GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name);

// Given a profile and a gdata_cache_path, return the file resource url.
void ModifyGDataFileResourceUrl(Profile* profile,
                                const FilePath& gdata_cache_path,
                                GURL* url);

// Returns true if the given path is under the GData mount point.
bool IsUnderGDataMountPoint(const FilePath& path);

// Extracts the GData path from the given path located under the GData mount
// point. Returns an empty path if |path| is not under the GData mount point.
// Examples: ExtractGDatPath("/special/gdata/foo.txt") => "gdata/foo.txt"
FilePath ExtractGDataPath(const FilePath& path);

// Grants read-only file access permissions to the process whose id is |pid|
// with ChildProcessSecurityPolicy for all possible cache paths that may be
// given to the specified file.
void SetPermissionsForGDataCacheFiles(Profile* profile,
                                      int pid,
                                      const FilePath& path);

// Returns true if gdata is currently active with the specified profile.
bool IsGDataAvailable(Profile* profile);
}  // namespace util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
