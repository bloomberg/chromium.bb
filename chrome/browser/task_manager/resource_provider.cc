// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/resource_provider.h"

#include "third_party/WebKit/public/web/WebCache.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace task_manager {

int Resource::GetRoutingID() const {
  return 0;
}

bool Resource::ReportsCacheStats() const {
  return false;
}

blink::WebCache::ResourceTypeStats Resource::GetWebCoreCacheStats() const {
  return blink::WebCache::ResourceTypeStats();
}

bool Resource::ReportsSqliteMemoryUsed() const {
  return false;
}

size_t Resource::SqliteMemoryUsedBytes() const {
  return 0;
}

bool Resource::ReportsV8MemoryStats() const {
  return false;
}

size_t Resource::GetV8MemoryAllocated() const {
  return 0;
}

size_t Resource::GetV8MemoryUsed() const {
  return 0;
}

content::WebContents* Resource::GetWebContents() const {
  return NULL;
}

}  // namespace task_manager
