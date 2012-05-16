// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/config_dir_policy_provider.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "chrome/browser/policy/policy_bundle.h"

namespace policy {

ConfigDirPolicyProviderDelegate::ConfigDirPolicyProviderDelegate(
    const FilePath& config_dir,
    PolicyLevel level,
    PolicyScope scope)
    : FileBasedPolicyProvider::ProviderDelegate(config_dir),
      level_(level),
      scope_(scope) {}

scoped_ptr<PolicyBundle> ConfigDirPolicyProviderDelegate::Load() {
  // Enumerate the files and sort them lexicographically.
  std::set<FilePath> files;
  file_util::FileEnumerator file_enumerator(config_file_path(), false,
                                            file_util::FileEnumerator::FILES);
  for (FilePath config_file_path = file_enumerator.Next();
       !config_file_path.empty(); config_file_path = file_enumerator.Next())
    files.insert(config_file_path);

  // Start with an empty dictionary and merge the files' contents.
  // The files are processed in reverse order because |MergeFrom| gives priority
  // to existing keys, but the ConfigDirPolicyProvider gives priority to the
  // last file in lexicographic order.
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  for (std::set<FilePath>::reverse_iterator config_file_iter = files.rbegin();
       config_file_iter != files.rend(); ++config_file_iter) {
    JSONFileValueSerializer deserializer(*config_file_iter);
    deserializer.set_allow_trailing_comma(true);
    int error_code = 0;
    std::string error_msg;
    scoped_ptr<Value> value(deserializer.Deserialize(&error_code, &error_msg));
    if (!value.get()) {
      LOG(WARNING) << "Failed to read configuration file "
                   << config_file_iter->value() << ": " << error_msg;
      continue;
    }
    DictionaryValue* dictionary_value = NULL;
    if (!value->GetAsDictionary(&dictionary_value)) {
      LOG(WARNING) << "Expected JSON dictionary in configuration file "
                   << config_file_iter->value();
      continue;
    }
    PolicyMap file_policy;
    file_policy.LoadFrom(dictionary_value, level_, scope_);
    bundle->Get(POLICY_DOMAIN_CHROME, std::string()).MergeFrom(file_policy);
  }

  return bundle.Pass();
}

base::Time ConfigDirPolicyProviderDelegate::GetLastModification() {
  base::Time last_modification = base::Time();
  base::PlatformFileInfo file_info;

  // If the path does not exist or points to a directory, it's safe to load.
  if (!file_util::GetFileInfo(config_file_path(), &file_info) ||
      !file_info.is_directory) {
    return last_modification;
  }

  // Enumerate the files and find the most recent modification timestamp.
  file_util::FileEnumerator file_enumerator(config_file_path(),
                                            false,
                                            file_util::FileEnumerator::FILES);
  for (FilePath config_file = file_enumerator.Next();
       !config_file.empty();
       config_file = file_enumerator.Next()) {
    if (file_util::GetFileInfo(config_file, &file_info) &&
        !file_info.is_directory) {
      last_modification = std::max(last_modification, file_info.last_modified);
    }
  }

  return last_modification;
}

ConfigDirPolicyProvider::ConfigDirPolicyProvider(
    const PolicyDefinitionList* policy_list,
    PolicyLevel level,
    PolicyScope scope,
    const FilePath& config_dir)
    : FileBasedPolicyProvider(
        policy_list,
        new ConfigDirPolicyProviderDelegate(config_dir, level, scope)) {}

}  // namespace policy
