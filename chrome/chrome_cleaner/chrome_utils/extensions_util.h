// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_

#include <vector>

#include "base/macros.h"
#include "chrome/chrome_cleaner/os/registry_util.h"

namespace chrome_cleaner {

// A registry key that holds some form of policy for |extension_id|.
struct ExtensionRegistryPolicy {
  base::string16 extension_id;
  HKEY hkey;
  base::string16 path;
  base::string16 name;

  ExtensionRegistryPolicy(const base::string16& extension_id,
                          HKEY hkey,
                          const base::string16& path,
                          const base::string16& name);
  ExtensionRegistryPolicy(ExtensionRegistryPolicy&&);
  ExtensionRegistryPolicy& operator=(ExtensionRegistryPolicy&&);

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistryPolicy);
};

// Find all extension forcelist registry policies and append to |policies|.
void GetExtensionForcelistRegistryPolicies(
    std::vector<ExtensionRegistryPolicy>* policies);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_EXTENSIONS_UTIL_H_
