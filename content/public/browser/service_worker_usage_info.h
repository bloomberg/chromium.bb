// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_USAGE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_USAGE_INFO_H_

#include <vector>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Used to report per-origin storage info for registered Service Workers.
class CONTENT_EXPORT ServiceWorkerUsageInfo {
 public:
  ServiceWorkerUsageInfo(const GURL& origin, const std::vector<GURL>& scopes);
  ServiceWorkerUsageInfo(const GURL& origin);
  ServiceWorkerUsageInfo();
  ~ServiceWorkerUsageInfo();

  // The origin this object is describing.
  GURL origin;

  // The set of all Service Worker registrations within this origin;
  // a registration is a 'scope' - a URL with optional wildcard.
  std::vector<GURL> scopes;

  // TODO(jsbell): Add size on disk (in bytes).
  // TODO(jsbell): Add last modified time.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_USAGE_INFO_H_
