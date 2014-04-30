// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_special_storage_policy.h"

#include "base/stl_util.h"

namespace content {

MockSpecialStoragePolicy::MockSpecialStoragePolicy()
    : all_unlimited_(false) {
}

bool MockSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return ContainsKey(protected_, origin);
}

bool MockSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (all_unlimited_)
    return true;
  return ContainsKey(unlimited_, origin);
}

bool MockSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return ContainsKey(session_only_, origin);
}

bool MockSpecialStoragePolicy::CanQueryDiskSize(const GURL& origin) {
  return ContainsKey(can_query_disk_size_, origin);
}

bool MockSpecialStoragePolicy::IsFileHandler(const std::string& extension_id) {
  return ContainsKey(file_handlers_, extension_id);
}

bool MockSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return ContainsKey(isolated_, origin);
}

bool MockSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}

MockSpecialStoragePolicy::~MockSpecialStoragePolicy() {}

}  // namespace content
