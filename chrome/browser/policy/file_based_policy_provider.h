// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_FILE_BASED_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_FILE_BASED_POLICY_PROVIDER_H_
#pragma once

#include "base/file_path.h"
#include "base/time.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"

namespace policy {

// File based policy provider that coordinates watching and reloading policy
// information from the configuration path. Actual logic for loading policy
// information is handled by a delegate passed at construction time.
class FileBasedPolicyProvider : public AsynchronousPolicyProvider {
 public:

  // Delegate interface for actual policy loading from the system.
  class ProviderDelegate : public AsynchronousPolicyProvider::Delegate {
   public:
    explicit ProviderDelegate(const FilePath& config_file_path);
    virtual ~ProviderDelegate();

    // AsynchronousPolicyProvider::Delegate implementation:
    virtual scoped_ptr<PolicyBundle> Load() = 0;

    // Gets the last modification timestamp for the policy information from the
    // filesystem. Returns base::Time() if the information is not present, in
    // which case Load() should return an empty dictionary.
    virtual base::Time GetLastModification() = 0;

    const FilePath& config_file_path() { return config_file_path_; }

   private:
    const FilePath config_file_path_;

    DISALLOW_COPY_AND_ASSIGN(ProviderDelegate);
  };

  // Assumes ownership of |delegate|.
  FileBasedPolicyProvider(const PolicyDefinitionList* policy_list,
                          ProviderDelegate* delegate);
  virtual ~FileBasedPolicyProvider() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_FILE_BASED_POLICY_PROVIDER_H_
