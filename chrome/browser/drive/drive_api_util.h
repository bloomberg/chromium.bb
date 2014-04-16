// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_
#define CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/drive/drive_entry_kinds.h"
#include "google_apis/drive/gdata_errorcode.h"

class GURL;

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace google_apis {
class AppList;
class AppResource;
class ChangeList;
class ChangeResource;
class DriveAppIcon;
class FileList;
class FileResource;
class ResourceEntry;
class ResourceList;
}  // namespace google_apis

namespace drive {
namespace util {

// Escapes ' to \' in the |str|. This is designed to use for string value of
// search parameter on Drive API v2.
// See also: https://developers.google.com/drive/search-parameters
std::string EscapeQueryStringValue(const std::string& str);

// Parses the query, and builds a search query for Drive API v2.
// This only supports:
//   Regular query (e.g. dog => fullText contains 'dog')
//   Conjunctions
//     (e.g. dog cat => fullText contains 'dog' and fullText contains 'cat')
//   Exclusion query (e.g. -cat => not fullText contains 'cat').
//   Quoted query (e.g. "dog cat" => fullText contains 'dog cat').
// See also: https://developers.google.com/drive/search-parameters
std::string TranslateQuery(const std::string& original_query);

// Extracts resource_id out of edit url.
std::string ExtractResourceIdFromUrl(const GURL& url);

// If |resource_id| is in the old resource ID format used by WAPI, converts it
// into the new format.
std::string CanonicalizeResourceId(const std::string& resource_id);

// Returns a ResourceIdCanonicalizer which returns the argument.
ResourceIdCanonicalizer GetIdentityResourceIdCanonicalizer();

// Note: Following constants and a function are used to support GetShareUrl on
// Drive API v2. Unfortunately, there is no support on Drive API v2, so we need
// to fall back to GData WAPI for the GetShareUrl. Thus, these are shared by
// both GDataWapiService and DriveAPIService.
// TODO(hidehiko): Remove these from here, when Drive API v2 supports
// GetShareUrl.

// OAuth2 scopes for the GData WAPI.
extern const char kDocsListScope[];
extern const char kDriveAppsScope[];

// Extracts an url to the sharing dialog and returns it via |callback|. If
// the share url doesn't exist, then an empty url is returned.
void ParseShareUrlAndRun(const google_apis::GetShareUrlCallback& callback,
                         google_apis::GDataErrorCode error,
                         scoped_ptr<base::Value> value);

// Converts ResourceEntry to FileResource.
scoped_ptr<google_apis::FileResource>
ConvertResourceEntryToFileResource(const google_apis::ResourceEntry& entry);

// Returns the GData WAPI's Kind of the FileResource.
google_apis::DriveEntryKind GetKind(
    const google_apis::FileResource& file_resource);

// Converts FileResource to ResourceEntry.
scoped_ptr<google_apis::ResourceEntry>
ConvertFileResourceToResourceEntry(
    const google_apis::FileResource& file_resource);

// Converts ChangeResource to ResourceEntry.
scoped_ptr<google_apis::ResourceEntry>
ConvertChangeResourceToResourceEntry(
    const google_apis::ChangeResource& change_resource);

// Converts FileList to ResourceList.
scoped_ptr<google_apis::ResourceList>
ConvertFileListToResourceList(const google_apis::FileList& file_list);

// Converts ChangeList to ResourceList.
scoped_ptr<google_apis::ResourceList>
ConvertChangeListToResourceList(const google_apis::ChangeList& change_list);

// Returns the (base-16 encoded) MD5 digest of the file content at |file_path|,
// or an empty string if an error is found.
std::string GetMd5Digest(const base::FilePath& file_path);

// The resource ID for the root directory for WAPI is defined in the spec:
// https://developers.google.com/google-apps/documents-list/
extern const char kWapiRootDirectoryResourceId[];

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_
