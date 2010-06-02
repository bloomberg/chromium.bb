// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/config_dir_policy_provider.h"

#include <set>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/json_value_serializer.h"

ConfigDirPolicyProvider::ConfigDirPolicyProvider(const FilePath& config_dir)
    : config_dir_(config_dir) {
}

bool ConfigDirPolicyProvider::Provide(ConfigurationPolicyStore* store) {
  scoped_ptr<DictionaryValue> policy(ReadPolicies());
  DecodePolicyValueTree(policy.get(), store);
  return true;
}

DictionaryValue* ConfigDirPolicyProvider::ReadPolicies() {
  // Enumerate the files and sort them lexicographically.
  std::set<FilePath> files;
  file_util::FileEnumerator file_enumerator(config_dir_, false,
                                            file_util::FileEnumerator::FILES);
  for (FilePath config_file_path = file_enumerator.Next();
       !config_file_path.empty(); config_file_path = file_enumerator.Next())
    files.insert(config_file_path);

  // Start with an empty dictionary and merge the files' contents.
  DictionaryValue* policy = new DictionaryValue;
  for (std::set<FilePath>::iterator config_file_iter = files.begin();
       config_file_iter != files.end(); ++config_file_iter) {
    JSONFileValueSerializer deserializer(*config_file_iter);
    int error_code = 0;
    std::string error_msg;
    scoped_ptr<Value> value(deserializer.Deserialize(&error_code, &error_msg));
    if (!value.get()) {
      LOG(WARNING) << "Failed to read configuration file "
                   << config_file_iter->value() << ": " << error_msg;
      continue;
    }
    if (!value->IsType(Value::TYPE_DICTIONARY)) {
      LOG(WARNING) << "Expected JSON dictionary in configuration file "
                   << config_file_iter->value();
      continue;
    }
    policy->MergeDictionary(static_cast<DictionaryValue*>(value.get()));
  }

  return policy;
}

void ConfigDirPolicyProvider::DecodePolicyValueTree(
    DictionaryValue* policies,
    ConfigurationPolicyStore* store) {
  const PolicyValueMap* mapping = PolicyValueMapping();
  for (PolicyValueMap::const_iterator i = mapping->begin();
       i != mapping->end(); ++i) {
    const PolicyValueMapEntry& entry(*i);
    Value* value;
    if (policies->Get(UTF8ToWide(entry.name), &value) &&
        value->IsType(entry.value_type))
      store->Apply(entry.policy_type, value->DeepCopy());
  }

  // TODO(mnissler): Handle preference overrides once |ConfigurationPolicyStore|
  // supports it.
}

