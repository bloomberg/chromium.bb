// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_namespace.h"

#include <string>

#include "base/strings/pattern.h"
#include "base/strings/string_util.h"

namespace content {

AppCacheNamespace::AppCacheNamespace()
    : type(APPCACHE_FALLBACK_NAMESPACE), is_pattern(false) {}

AppCacheNamespace::AppCacheNamespace(AppCacheNamespaceType type,
                                     const GURL& url,
                                     const GURL& target,
                                     bool is_pattern)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern) {}

AppCacheNamespace::~AppCacheNamespace() = default;

bool AppCacheNamespace::IsMatch(const GURL& url) const {
  if (is_pattern) {
    // We have to escape '?' characters since MatchPattern also treats those
    // as wildcards which we don't want here, we only do '*'s.
    std::string pattern = namespace_url.spec();
    if (namespace_url.has_query())
      base::ReplaceSubstringsAfterOffset(&pattern, 0, "?", "\\?");
    return base::MatchPattern(url.spec(), pattern);
  }
  return base::StartsWith(url.spec(), namespace_url.spec(),
                          base::CompareCase::SENSITIVE);
}

}  // namespace content