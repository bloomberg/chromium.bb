// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
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
  DCHECK(!scope.has_ref());
  DCHECK(!url.has_ref());
  const std::string& scope_spec = scope.spec();
  const std::string& url_spec = url.spec();

  size_t len = scope_spec.size();
  if (len > 0 && scope_spec[len - 1] == '*')
    return scope_spec.compare(0, len - 1, url_spec, 0, len - 1) == 0;
  return scope_spec == url_spec;
}

}  // namespace content
