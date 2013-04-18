// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_UTIL_H_

#include <string>

namespace google_apis {
namespace util {

// Returns true if Drive v2 API is enabled via commandline switch.
bool IsDriveV2ApiEnabled();

}  // namespace util

namespace drive {
namespace util {

// Escapes ' to \' in the |str|. This is designed to use for string value of
// search parameter on Drive API v2.
// See also: https://developers.google.com/drive/search-parameters
std::string EscapeQueryStringValue(const std::string& str);

}  // namespace util
}  // namespace drive
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_UTIL_H_
