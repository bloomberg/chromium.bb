// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the URLMatcher component of the Declarative API.

#ifndef CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_CONSTANTS_H_
#pragma once

namespace extensions {
namespace url_matcher_constants {

// Keys of dictionaries for URL constraints
extern const char kPortsKey[];
extern const char kSchemesKey[];
extern const char kHostContainsKey[];
extern const char kHostEqualsKey[];
extern const char kHostPrefixKey[];
extern const char kHostSuffixKey[];
extern const char kHostSuffixPathPrefixKey[];
extern const char kPathContainsKey[];
extern const char kPathEqualsKey[];
extern const char kPathPrefixKey[];
extern const char kPathSuffixKey[];
extern const char kQueryContainsKey[];
extern const char kQueryEqualsKey[];
extern const char kQueryPrefixKey[];
extern const char kQuerySuffixKey[];
extern const char kURLContainsKey[];
extern const char kURLEqualsKey[];
extern const char kURLPrefixKey[];
extern const char kURLSuffixKey[];

}  // namespace url_matcher_constants
}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_CONSTANTS_H_
