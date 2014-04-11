// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool ServiceWorkerUtils::IsFeatureEnabled() {
  static bool enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableServiceWorker);
  return enabled;
}

// static
bool ServiceWorkerUtils::ScopeMatches(const GURL& scope, const GURL& url) {
  // This is a really basic, naive
  // TODO(alecflett): Formalize what scope matches mean.
  // Temporarily borrowed directly from appcache::Namespace::IsMatch().
  // We have to escape '?' characters since MatchPattern also treats those
  // as wildcards which we don't want here, we only do '*'s.
  std::string scope_spec(scope.spec());
  if (scope.has_query())
    ReplaceSubstringsAfterOffset(&scope_spec, 0, "?", "\\?");
  return MatchPattern(url.spec(), scope_spec);
}

}  // namespace content
