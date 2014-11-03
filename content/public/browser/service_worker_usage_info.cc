// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_usage_info.h"

namespace content {

ServiceWorkerUsageInfo::ServiceWorkerUsageInfo(const GURL& origin,
                                               const std::vector<GURL>& scopes)
    : origin(origin), scopes(scopes), total_size_bytes(0) {
}

ServiceWorkerUsageInfo::ServiceWorkerUsageInfo(const GURL& origin)
    : origin(origin), total_size_bytes(0) {
}

ServiceWorkerUsageInfo::ServiceWorkerUsageInfo() : total_size_bytes(0) {
}

ServiceWorkerUsageInfo::~ServiceWorkerUsageInfo() {
}

}  // namespace content
