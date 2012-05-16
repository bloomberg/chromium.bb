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
#include "base/stl_util.h"
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
    scoped_ptr<base::Value> value(
        deserializer.Deserialize(&error_code, &error_msg));
    if (!value.get()) {
      LOG(WARNING) << "Failed to read configuration file "
                   << config_file_iter->value() << ": " << error_msg;
      continue;
    }
    base::DictionaryValue* dictionary_value = NULL;
    if (!value->GetAsDictionary(&dictionary_value)) {
      LOG(WARNING) << "Expected JSON dictionary in configuration file "
                   << config_file_iter->value();
      continue;
    }

    // Detach the "3rdparty" node.
    base::Value* third_party = NULL;
    if (dictionary_value->Remove("3rdparty", &third_party)) {
      Merge3rdPartyPolicy(bundle.get(), third_party);
      delete third_party;
    }

    // Add chrome policy.
    PolicyMap policy_map;
    policy_map.LoadFrom(dictionary_value, level_, scope_);
    bundle->Get(POLICY_DOMAIN_CHROME, "").MergeFrom(policy_map);
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

void ConfigDirPolicyProviderDelegate::Merge3rdPartyPolicy(
    PolicyBundle* bundle,
    const base::Value* value) {
  // The first-level entries in |value| are PolicyDomains. The second-level
  // entries are component IDs, and the third-level entries are the policies
  // for that domain/component namespace.

  const base::DictionaryValue* domains_dictionary;
  if (!value->GetAsDictionary(&domains_dictionary)) {
    LOG(WARNING) << "3rdparty value is not a dictionary!";
    return;
  }

  // Helper to lookup a domain given its string name.
  std::map<std::string, PolicyDomain> supported_domains;
  supported_domains["extensions"] = POLICY_DOMAIN_EXTENSIONS;

  for (base::DictionaryValue::Iterator domains_it(*domains_dictionary);
       domains_it.HasNext(); domains_it.Advance()) {
    if (!ContainsKey(supported_domains, domains_it.key())) {
      LOG(WARNING) << "Unsupported 3rd party policy domain: "
                   << domains_it.key();
      continue;
    }

    const base::DictionaryValue* components_dictionary;
    if (!domains_it.value().GetAsDictionary(&components_dictionary)) {
      LOG(WARNING) << "3rdparty/" << domains_it.key()
                   << " value is not a dictionary!";
      continue;
    }

    PolicyDomain domain = supported_domains[domains_it.key()];
    for (base::DictionaryValue::Iterator components_it(*components_dictionary);
         components_it.HasNext(); components_it.Advance()) {
      const base::DictionaryValue* policy_dictionary;
      if (!components_it.value().GetAsDictionary(&policy_dictionary)) {
        LOG(WARNING) << "3rdparty/" << domains_it.key() << "/"
                     << components_it.key() << " value is not a dictionary!";
        continue;
      }

      PolicyMap policy;
      policy.LoadFrom(policy_dictionary, level_, scope_);
      bundle->Get(domain, components_it.key()).MergeFrom(policy);
    }
  }
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
