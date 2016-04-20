// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_WIN_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_WIN_H_

#include <windows.h>
#include <userenv.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class AppliedGPOListProvider;
class PolicyLoadStatusSample;
class PolicyMap;
class RegistryDict;

// Interface for mocking out GPO enumeration in tests.
class POLICY_EXPORT AppliedGPOListProvider {
 public:
  virtual ~AppliedGPOListProvider() {}
  virtual DWORD GetAppliedGPOList(DWORD flags,
                                  LPCTSTR machine_name,
                                  PSID sid_user,
                                  GUID* extension_guid,
                                  PGROUP_POLICY_OBJECT* gpo_list) = 0;
  virtual BOOL FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) = 0;
};

// Loads policies from the Windows registry, and watches for Group Policy
// notifications to trigger reloads.
class POLICY_EXPORT PolicyLoaderWin
    : public AsyncPolicyLoader,
      public base::win::ObjectWatcher::Delegate {
 public:
  // The PReg file name used by GPO.
  static const base::FilePath::CharType kPRegFileName[];

  // Passing |gpo_provider| equal nullptr forces all reads to go through the
  // registry.  This is undesirable for Chrome (see crbug.com/259236), but
  // needed for some other use cases (i.e. Chromoting - see crbug.com/460734).
  PolicyLoaderWin(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  const base::string16& chrome_policy_key,
                  AppliedGPOListProvider* gpo_provider);
  ~PolicyLoaderWin() override;

  // Creates a policy loader that uses the Win API to access GPO.
  static std::unique_ptr<PolicyLoaderWin> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::string16& chrome_policy_key);

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  std::unique_ptr<PolicyBundle> Load() override;

 private:
  // Reads Chrome Policy from a PReg file at the given path and stores the
  // result in |policy|.
  bool ReadPRegFile(const base::FilePath& preg_file,
                    RegistryDict* policy,
                    PolicyLoadStatusSample* status);

  // Loads and parses GPO policy in |policy_object_list| for scope |scope|. If
  // successful, stores the result in |policy| and returns true. Returns false
  // on failure reading the policy, indicating that policy loading should fall
  // back to reading the registry.
  bool LoadGPOPolicy(PolicyScope scope,
                     PGROUP_POLICY_OBJECT policy_object_list,
                     RegistryDict* policy,
                     PolicyLoadStatusSample* status);

  // Queries Windows for applied group policy and writes the result to |policy|.
  // This is the preferred way to obtain GPO data, there are reports of abuse
  // of the registry GPO keys by 3rd-party software.
  bool ReadPolicyFromGPO(PolicyScope scope,
                         RegistryDict* policy,
                         PolicyLoadStatusSample* status);

  // Parses Chrome policy from |gpo_dict| for the given |scope| and |level| and
  // merges it into |chrome_policy_map|.
  void LoadChromePolicy(const RegistryDict* gpo_dict,
                        PolicyLevel level,
                        PolicyScope scope,
                        PolicyMap* chrome_policy_map);

  // Loads 3rd-party policy from |gpo_dict| and merges it into |bundle|.
  void Load3rdPartyPolicy(const RegistryDict* gpo_dict,
                          PolicyScope scope,
                          PolicyBundle* bundle);

  // Installs the watchers for the Group Policy update events.
  void SetupWatches();

  // ObjectWatcher::Delegate overrides:
  void OnObjectSignaled(HANDLE object) override;

  bool is_initialized_;
  const base::string16 chrome_policy_key_;
  class AppliedGPOListProvider* gpo_provider_;

  base::WaitableEvent user_policy_changed_event_;
  base::WaitableEvent machine_policy_changed_event_;
  base::win::ObjectWatcher user_policy_watcher_;
  base::win::ObjectWatcher machine_policy_watcher_;
  bool user_policy_watcher_failed_;
  bool machine_policy_watcher_failed_;

  DISALLOW_COPY_AND_ASSIGN(PolicyLoaderWin);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_WIN_H_
