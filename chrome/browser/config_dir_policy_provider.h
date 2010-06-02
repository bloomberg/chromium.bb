// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFIG_DIR_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CONFIG_DIR_POLICY_PROVIDER_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/configuration_policy_provider.h"

class DictionaryValue;

// A policy provider implementation backed by a set of files in a given
// directory. The files should contain JSON-formatted policy settings. They are
// merged together and the result is returned via the
// ConfigurationPolicyProvider interface. The files are consulted in
// lexicographic file name order, so the last value read takes precedence in
// case of preference key collisions.
class ConfigDirPolicyProvider : public ConfigurationPolicyProvider {
 public:
  explicit ConfigDirPolicyProvider(const FilePath& config_dir);
  virtual ~ConfigDirPolicyProvider() { }

  // ConfigurationPolicyProvider implementation.
  virtual bool Provide(ConfigurationPolicyStore* store);

 private:
  // Read and merge the files from the configuration directory.
  DictionaryValue* ReadPolicies();

  // Decodes the value tree and writes the configuration to the given |store|.
  void DecodePolicyValueTree(DictionaryValue* policies,
                             ConfigurationPolicyStore* store);

  // The directory in which we look for configuration files.
  const FilePath config_dir_;

  DISALLOW_COPY_AND_ASSIGN(ConfigDirPolicyProvider);
};

#endif  // CHROME_BROWSER_CONFIG_DIR_POLICY_PROVIDER_H_
