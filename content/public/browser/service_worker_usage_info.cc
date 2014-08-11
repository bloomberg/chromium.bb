// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_usage_info.h"

namespace content {

ServiceWorkerUsageInfo::ServiceWorkerUsageInfo(const GURL& origin,
                                               const std::vector<GURL>& scopes)
    : origin(origin), scopes(scopes) {
}

ServiceWorkerUsageInfo::ServiceWorkerUsageInfo(const GURL& origin)
    : origin(origin) {
}

ServiceWorkerUsageInfo::ServiceWorkerUsageInfo() {
}

ServiceWorkerUsageInfo::~ServiceWorkerUsageInfo() {
}

}  // namespace content
