// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/file_based_policy_provider.h"

#include "chrome/browser/policy/file_based_policy_loader.h"

namespace policy {

FileBasedPolicyProvider::ProviderDelegate::ProviderDelegate(
    const FilePath& config_file_path)
    : config_file_path_(config_file_path) {}

FileBasedPolicyProvider::ProviderDelegate::~ProviderDelegate() {}

FileBasedPolicyProvider::FileBasedPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    FileBasedPolicyProvider::ProviderDelegate* delegate)
    : AsynchronousPolicyProvider(
        policy_list,
        new FileBasedPolicyLoader(delegate)) {}

}  // namespace policy
