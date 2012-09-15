// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIG_DIR_POLICY_LOADER_H_
#define CHROME_BROWSER_POLICY_CONFIG_DIR_POLICY_LOADER_H_

#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "chrome/browser/policy/async_policy_loader.h"
#include "chrome/browser/policy/policy_types.h"

namespace base {
class Value;
}

namespace policy {

// A policy loader implementation backed by a set of files in a given
// directory. The files should contain JSON-formatted policy settings. They are
// merged together and the result is returned in a PolicyBundle.
// The files are consulted in lexicographic file name order, so the
// last value read takes precedence in case of policy key collisions.
class ConfigDirPolicyLoader : public AsyncPolicyLoader {
 public:
  ConfigDirPolicyLoader(const FilePath& config_dir, PolicyScope scope);
  virtual ~ConfigDirPolicyLoader();

  // AsyncPolicyLoader implementation.
  virtual void InitOnFile() OVERRIDE;
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE;
  virtual base::Time LastModificationTime() OVERRIDE;

 private:
  // Loads the policy files at |path| into the |bundle|, with the given |level|.
  void LoadFromPath(const FilePath& path,
                    PolicyLevel level,
                    PolicyBundle* bundle);

  // Merges the 3rd party |policies| into the |bundle|, with the given |level|.
  void Merge3rdPartyPolicy(const base::Value* policies,
                           PolicyLevel level,
                           PolicyBundle* bundle);

  // Callback for the FilePathWatchers.
  void OnFileUpdated(const FilePath& path, bool error);

  // The directory containing the policy files.
  FilePath config_dir_;

  // Policies loaded by this provider will have this scope.
  PolicyScope scope_;

  // Watchers for events on the mandatory and recommended subdirectories of
  // |config_dir_|.
  base::files::FilePathWatcher mandatory_watcher_;
  base::files::FilePathWatcher recommended_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ConfigDirPolicyLoader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIG_DIR_POLICY_LOADER_H_
