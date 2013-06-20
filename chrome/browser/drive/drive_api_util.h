// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_
#define CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_

#include <string>

class GURL;

namespace drive {
namespace util {

// Returns true if Drive v2 API is enabled via commandline switch.
bool IsDriveV2ApiEnabled();

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

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_API_UTIL_H_
