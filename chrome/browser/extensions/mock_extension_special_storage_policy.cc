// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"

MockExtensionSpecialStoragePolicy::MockExtensionSpecialStoragePolicy()
    : ExtensionSpecialStoragePolicy(NULL) {}

MockExtensionSpecialStoragePolicy::~MockExtensionSpecialStoragePolicy() {}

bool MockExtensionSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return protected_.find(origin) != protected_.end();
}

bool MockExtensionSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  return unlimited_.find(origin) != unlimited_.end();
}

bool MockExtensionSpecialStoragePolicy::IsStorageSessionOnly(
    const GURL& origin) {
  return session_only_.find(origin) != session_only_.end();
}

bool MockExtensionSpecialStoragePolicy::IsFileHandler(
    const std::string& extension_id) {
  return file_handlers_.find(extension_id) != file_handlers_.end();
}

bool MockExtensionSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}
