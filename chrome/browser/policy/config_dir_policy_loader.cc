// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/config_dir_policy_loader.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/policy_bundle.h"

namespace policy {

namespace {

// Subdirectories that contain the mandatory and recommended policies.
const base::FilePath::CharType kMandatoryConfigDir[] =
    FILE_PATH_LITERAL("managed");
const base::FilePath::CharType kRecommendedConfigDir[] =
    FILE_PATH_LITERAL("recommended");

}  // namespace

ConfigDirPolicyLoader::ConfigDirPolicyLoader(const base::FilePath& config_dir,
                                             PolicyScope scope)
    : config_dir_(config_dir),
      scope_(scope) {}

ConfigDirPolicyLoader::~ConfigDirPolicyLoader() {}

void ConfigDirPolicyLoader::InitOnFile() {
  base::FilePathWatcher::Callback callback =
      base::Bind(&ConfigDirPolicyLoader::OnFileUpdated, base::Unretained(this));
  mandatory_watcher_.Watch(config_dir_.Append(kMandatoryConfigDir), false,
                           callback);
  recommended_watcher_.Watch(config_dir_.Append(kRecommendedConfigDir), false,
                             callback);
}

scoped_ptr<PolicyBundle> ConfigDirPolicyLoader::Load() {
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  LoadFromPath(config_dir_.Append(kMandatoryConfigDir),
               POLICY_LEVEL_MANDATORY,
               bundle.get());
  LoadFromPath(config_dir_.Append(kRecommendedConfigDir),
               POLICY_LEVEL_RECOMMENDED,
               bundle.get());
  return bundle.Pass();
}

base::Time ConfigDirPolicyLoader::LastModificationTime() {
  static const base::FilePath::CharType* kConfigDirSuffixes[] = {
    kMandatoryConfigDir,
    kRecommendedConfigDir,
  };

  base::Time last_modification = base::Time();
  base::PlatformFileInfo info;

  for (size_t i = 0; i < arraysize(kConfigDirSuffixes); ++i) {
    base::FilePath path(config_dir_.Append(kConfigDirSuffixes[i]));

    // Skip if the file doesn't exist, or it isn't a directory.
    if (!file_util::GetFileInfo(path, &info) || !info.is_directory)
      continue;

    // Enumerate the files and find the most recent modification timestamp.
    file_util::FileEnumerator file_enumerator(path, false,
                                              file_util::FileEnumerator::FILES);
    for (base::FilePath config_file = file_enumerator.Next();
         !config_file.empty();
         config_file = file_enumerator.Next()) {
      if (file_util::GetFileInfo(config_file, &info) && !info.is_directory)
        last_modification = std::max(last_modification, info.last_modified);
    }
  }

  return last_modification;
}

void ConfigDirPolicyLoader::LoadFromPath(const base::FilePath& path,
                                         PolicyLevel level,
                                         PolicyBundle* bundle) {
  // Enumerate the files and sort them lexicographically.
  std::set<base::FilePath> files;
  file_util::FileEnumerator file_enumerator(path, false,
                                            file_util::FileEnumerator::FILES);
  for (base::FilePath config_file_path = file_enumerator.Next();
       !config_file_path.empty(); config_file_path = file_enumerator.Next())
    files.insert(config_file_path);

  // Start with an empty dictionary and merge the files' contents.
  // The files are processed in reverse order because |MergeFrom| gives priority
  // to existing keys, but the ConfigDirPolicyProvider gives priority to the
  // last file in lexicographic order.
  for (std::set<base::FilePath>::reverse_iterator config_file_iter =
           files.rbegin(); config_file_iter != files.rend();
       ++config_file_iter) {
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
      Merge3rdPartyPolicy(third_party, level, bundle);
      delete third_party;
    }

    // Add chrome policy.
    PolicyMap policy_map;
    policy_map.LoadFrom(dictionary_value, level, scope_);
    bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .MergeFrom(policy_map);
  }
}

void ConfigDirPolicyLoader::Merge3rdPartyPolicy(
    const base::Value* policies,
    PolicyLevel level,
    PolicyBundle* bundle) {
  // The first-level entries in |policies| are PolicyDomains. The second-level
  // entries are component IDs, and the third-level entries are the policies
  // for that domain/component namespace.

  const base::DictionaryValue* domains_dictionary;
  if (!policies->GetAsDictionary(&domains_dictionary)) {
    LOG(WARNING) << "3rdparty value is not a dictionary!";
    return;
  }

  // Helper to lookup a domain given its string name.
  std::map<std::string, PolicyDomain> supported_domains;
  supported_domains["extensions"] = POLICY_DOMAIN_EXTENSIONS;

  for (base::DictionaryValue::Iterator domains_it(*domains_dictionary);
       !domains_it.IsAtEnd(); domains_it.Advance()) {
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
         !components_it.IsAtEnd(); components_it.Advance()) {
      const base::DictionaryValue* policy_dictionary;
      if (!components_it.value().GetAsDictionary(&policy_dictionary)) {
        LOG(WARNING) << "3rdparty/" << domains_it.key() << "/"
                     << components_it.key() << " value is not a dictionary!";
        continue;
      }

      PolicyMap policy;
      policy.LoadFrom(policy_dictionary, level, scope_);
      bundle->Get(PolicyNamespace(domain, components_it.key()))
          .MergeFrom(policy);
    }
  }
}

void ConfigDirPolicyLoader::OnFileUpdated(const base::FilePath& path,
                                          bool error) {
  if (!error)
    Reload(false);
}

}  // namespace policy
