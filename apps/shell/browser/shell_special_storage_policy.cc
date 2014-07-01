// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_special_storage_policy.h"

namespace apps {

ShellSpecialStoragePolicy::ShellSpecialStoragePolicy() {
}

ShellSpecialStoragePolicy::~ShellSpecialStoragePolicy() {
}

bool ShellSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return true;
}

bool ShellSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  return true;
}

bool ShellSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return false;
}

bool ShellSpecialStoragePolicy::CanQueryDiskSize(const GURL& origin) {
  return true;
}

bool ShellSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return false;
}

bool ShellSpecialStoragePolicy::IsFileHandler(const std::string& extension_id) {
  return true;
}

bool ShellSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return false;
}

}  // namespace apps
