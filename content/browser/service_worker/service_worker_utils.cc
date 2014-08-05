// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace content {

// static
bool ServiceWorkerUtils::ScopeMatches(const GURL& scope, const GURL& url) {
  DCHECK(!scope.has_ref());
  DCHECK(!url.has_ref());
  return StartsWithASCII(url.spec(), scope.spec(), true);
}

bool LongestScopeMatcher::MatchLongest(const GURL& scope) {
  if (!ServiceWorkerUtils::ScopeMatches(scope, url_))
    return false;
  if (match_.is_empty() || match_.spec().size() < scope.spec().size()) {
    match_ = scope;
    return true;
  }
  return false;
}

}  // namespace content
