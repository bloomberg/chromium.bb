// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"

class FilePath;
class Profile;

namespace gdata {
namespace util {

// Search path is a path used to display gdata content search results.
// All results are displayed under virtual directory "drive/.search", in which
// each query is given its own directory for displaying results.
enum GDataSearchPathType {
  // Not a search path.
  GDATA_SEARCH_PATH_INVALID,
  // drive/.search.
  GDATA_SEARCH_PATH_ROOT,
  // Path that defines search query (drive/.search/foo).
  GDATA_SEARCH_PATH_QUERY,
  // Path given to a search result (drive/.search/foo/foo_found).
  // The file name will be formatted: "resource_id.file_name".
  GDATA_SEARCH_PATH_RESULT,
  // If search result is directory, it may contain some children.
  GDATA_SEARCH_PATH_RESULT_CHILD
};

// Returns the GData mount point path, which looks like "/special/gdata".
const FilePath& GetGDataMountPointPath();

// Returns the GData mount path as string.
const std::string& GetGDataMountPointPathAsString();

// Returns the 'local' root of remote file system as "/special".
const FilePath& GetSpecialRemoteRootPath();

// Returns the gdata file resource url formatted as
// chrome://drive/<resource_id>/<file_name>.
GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name);

// Given a profile and a gdata_cache_path, return the file resource url.
void ModifyGDataFileResourceUrl(Profile* profile,
                                const FilePath& gdata_cache_path,
                                GURL* url);

// Returns true if the given path is under the GData mount point.
bool IsUnderGDataMountPoint(const FilePath& path);

// Checks if the path is under (virtual) gdata search directory, and returns its
// search status.
GDataSearchPathType GetSearchPathStatus(const FilePath& path);

// Checks if the path is under (virtual) gdata earch directory, and returns its
// search status.
GDataSearchPathType GetSearchPathStatusForPathComponents(
    const std::vector<std::string>& path_components);

// Gets resource id and original file name from the search file name.
// Search file name is formatted as: <resource_id>.<original_file_name>.
// If the path is not search path, the behaviour is not defined.
bool ParseSearchFileName(const std::string& search_file_name,
                         std::string* resource_id,
                         std::string* original_file_name);

// Extracts the GData path from the given path located under the GData mount
// point. Returns an empty path if |path| is not under the GData mount point.
// Examples: ExtractGDatPath("/special/drive/foo.txt") => "drive/foo.txt"
FilePath ExtractGDataPath(const FilePath& path);

// Returns vector of all possible cache paths for a given path on gdata mount
// point.
void InsertGDataCachePathsPermissions(
    Profile* profile_,
    const FilePath& gdata_path,
    std::vector<std::pair<FilePath, int> >* cache_paths);

// Returns true if gdata is currently active with the specified profile.
bool IsGDataAvailable(Profile* profile);

// Escapes a file name in GData cache.
// Replaces percent ('%'), period ('.') and slash ('/') with %XX (hex)
std::string EscapeCacheFileName(const std::string& filename);

// Unescapes a file path in GData cache.
// This is the inverse of EscapeCacheFileName.
std::string UnescapeCacheFileName(const std::string& filename);

// Extracts resource_id, md5, and extra_extension from cache path.
// Case 1: Pinned and outgoing symlinks only have resource_id.
// Example: path="/user/GCache/v1/pinned/pdf:a1b2" =>
//          resource_id="pdf:a1b2", md5="", extra_extension="";
// Case 2: Normal files have both resource_id and md5.
// Example: path="/user/GCache/v1/tmp/pdf:a1b2.01234567" =>
//          resource_id="pdf:a1b2", md5="01234567", extra_extension="";
// Case 3: Mounted files have all three parts.
// Example: path="/user/GCache/v1/persistent/pdf:a1b2.01234567.mounted" =>
//          resource_id="pdf:a1b2", md5="01234567", extra_extension="mounted".
void ParseCacheFilePath(const FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension);
}  // namespace util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
