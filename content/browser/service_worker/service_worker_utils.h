// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

#include "content/common/content_export.h"
#include "webkit/common/resource_type.h"

namespace content {

class ServiceWorkerUtils {
 public:
  static bool IsMainResourceType(ResourceType::Type type) {
    return ResourceType::IsFrame(type) ||
           ResourceType::IsSharedWorker(type);
  }

  static bool IsServiceWorkerResourceType(ResourceType::Type type) {
    return ResourceType::IsServiceWorker(type);
  }

  // Returns true if the feature is enabled (or not disabled) by command-line
  // flag.
  static bool IsFeatureEnabled();

  // Returns true if |scope| matches |url|.
  CONTENT_EXPORT static bool ScopeMatches(const GURL& scope, const GURL& url);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
