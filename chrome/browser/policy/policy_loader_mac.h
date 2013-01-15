// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_LOADER_MAC_H_
#define CHROME_BROWSER_POLICY_POLICY_LOADER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "chrome/browser/policy/async_policy_loader.h"

class MacPreferences;

namespace base {
class Value;
}  // namespace base

namespace policy {

struct PolicyDefinitionList;

// A policy loader that loads policies from the Mac preferences system, and
// watches the managed preferences files for updates.
class PolicyLoaderMac : public AsyncPolicyLoader {
 public:
  PolicyLoaderMac(const PolicyDefinitionList* policy_list,
                  MacPreferences* preferences);
  virtual ~PolicyLoaderMac();

  // AsyncPolicyLoader implementation.
  virtual void InitOnFile() OVERRIDE;
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE;
  virtual base::Time LastModificationTime() OVERRIDE;

  // Converts a CFPropertyListRef to the equivalent base::Value. CFDictionary
  // entries whose key is not a CFStringRef are ignored.
  // The returned value is owned by the caller.
  // Returns NULL if an invalid CFType was found, such as CFDate or CFData.
  static base::Value* CreateValueFromProperty(CFPropertyListRef property);

 private:
  // Callback for the FilePathWatcher.
  void OnFileUpdated(const FilePath& path, bool error);

  // List of recognized policies.
  const PolicyDefinitionList* policy_list_;

  scoped_ptr<MacPreferences> preferences_;

  // Path to the managed preferences file for the current user, if it could
  // be found. Updates of this file trigger a policy reload.
  FilePath managed_policy_path_;

  // Watches for events on the |managed_policy_path_|.
  base::FilePathWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(PolicyLoaderMac);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_LOADER_MAC_H_
