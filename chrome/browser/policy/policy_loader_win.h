// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_LOADER_WIN_H_
#define CHROME_BROWSER_POLICY_POLICY_LOADER_WIN_H_
#pragma once

#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "chrome/browser/policy/async_policy_loader.h"

namespace policy {

struct PolicyDefinitionList;
class PolicyMap;

// Loads policies from the Windows registry, and watches for Group Policy
// notifications to trigger reloads.
class PolicyLoaderWin : public AsyncPolicyLoader,
                        public base::win::ObjectWatcher::Delegate {
 public:
  explicit PolicyLoaderWin(const PolicyDefinitionList* policy_list);
  virtual ~PolicyLoaderWin();

  // AsyncPolicyLoader implementation.
  virtual void InitOnFile() OVERRIDE;
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE;

 private:
  void LoadChromePolicy(PolicyMap* chrome_policies);
  void Load3rdPartyPolicies(PolicyBundle* bundle);

  // Installs the watchers for the Group Policy update events.
  void SetupWatches();

  // ObjectWatcher::Delegate overrides:
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  bool is_initialized_;
  const PolicyDefinitionList* policy_list_;

  base::WaitableEvent user_policy_changed_event_;
  base::WaitableEvent machine_policy_changed_event_;
  base::win::ObjectWatcher user_policy_watcher_;
  base::win::ObjectWatcher machine_policy_watcher_;
  bool user_policy_watcher_failed_;
  bool machine_policy_watcher_failed_;

  DISALLOW_COPY_AND_ASSIGN(PolicyLoaderWin);
};

// Constants shared with tests.
namespace registry_constants {
  // Path separator for registry keys.
  extern const wchar_t kPathSep[];
  // Registry key within Chrome's key that contains 3rd party policy.
  extern const wchar_t kThirdParty[];
  // Registry key within an extension's namespace that contains mandatory
  // policy.
  extern const wchar_t kMandatory[];
  // Registry key within an extension's namespace that contains recommended
  // policy.
  extern const wchar_t kRecommended[];
  // Registry key within an extension's namespace that contains the policy
  // schema.
  extern const wchar_t kSchema[];
  // Key in a JSON schema that indicates the expected type.
  extern const char kType[];
  // Key in a JSON schema that indicates the expected properties of an object.
  extern const char kProperties[];
  // Key in a JSON schema that indicates the default schema for object
  // properties.
  extern const char kAdditionalProperties[];
  // Key in a JSON schema that indicates the expected type of list items.
  extern const char kItems[];
}  // namespace registry_constants

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_LOADER_WIN_H_
