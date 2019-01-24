// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_APPCACHE_INTERFACES_H_
#define CONTENT_COMMON_APPCACHE_INTERFACES_H_

#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Temporarily renumber them in wierd way, to help remove LOG_TIP from WebKit
enum AppCacheLogLevel {
  APPCACHE_LOG_VERBOSE,
  APPCACHE_LOG_INFO,
  APPCACHE_LOG_WARNING,
  APPCACHE_LOG_ERROR
};

// Useful string constants.
CONTENT_EXPORT extern const char kHttpGETMethod[];
CONTENT_EXPORT extern const char kHttpHEADMethod[];

CONTENT_EXPORT bool IsSchemeSupportedForAppCache(const GURL& url);
CONTENT_EXPORT bool IsMethodSupportedForAppCache(const std::string& method);

}  // namespace

#endif  // CONTENT_COMMON_APPCACHE_INTERFACES_H_
