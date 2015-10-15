// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_

#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

class Profile;

namespace extensions {

class ExtensionSystemSharedFactory;
class NavigationObserver;
class StateStoreNotificationObserver;

// The ExtensionSystem for ProfileImpl and OffTheRecordProfileImpl.
// Implementation details: non-shared services are owned by
// ExtensionSystemImpl, a KeyedService with separate incognito
// instances. A private Shared class (also a KeyedService,
// but with a shared instance for incognito) keeps the common services.
class ExtensionSystemImpl : public ExtensionSystem {
 public:
  explicit ExtensionSystemImpl(Profile* profile);
  ~ExtensionSystemImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  void InitForRegularProfile(bool extensions_enabled) override;

  ExtensionService* extension_service() override;  // shared
  RuntimeData* runtime_data() override;            // shared
  ManagementPolicy* management_policy() override;  // shared
  ServiceWorkerManager* service_worker_manager() override;  // shared
  SharedUserScriptMaster* shared_user_script_master() override;  // shared
  StateStore* state_store() override;                              // shared
  StateStore* rules_store() override;                              // shared
  InfoMap* info_map() override;                                    // shared
  QuotaService* quota_service() override;  // shared
  AppSorting* app_sorting() override;  // shared

  void RegisterExtensionWithRequestContexts(
      const Extension* extension,
      const base::Closure& callback) override;

  void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) override;

  const OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;  // shared
  scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const base::FilePath& temp_dir) override;

 private:
  friend class ExtensionSystemSharedFactory;

  // Owns the Extension-related systems that have a single instance
  // shared between normal and incognito profiles.
  class Shared : public KeyedService {
   public:
    explicit Shared(Profile* profile);
    ~Shared() override;

    // Initialization takes place in phases.
    virtual void InitPrefs();
    // This must not be called until all the providers have been created.
    void RegisterManagementPolicyProviders();
    void Init(bool extensions_enabled);

    // KeyedService implementation.
    void Shutdown() override;

    StateStore* state_store();
    StateStore* rules_store();
    ExtensionService* extension_service();
    RuntimeData* runtime_data();
    ManagementPolicy* management_policy();
    ServiceWorkerManager* service_worker_manager();
    SharedUserScriptMaster* shared_user_script_master();
    InfoMap* info_map();
    QuotaService* quota_service();
    AppSorting* app_sorting();
    const OneShotEvent& ready() const { return ready_; }
    ContentVerifier* content_verifier();

   private:
    Profile* profile_;

    // The services that are shared between normal and incognito profiles.

    scoped_ptr<StateStore> state_store_;
    scoped_ptr<StateStoreNotificationObserver>
        state_store_notification_observer_;
    scoped_ptr<StateStore> rules_store_;
    scoped_ptr<NavigationObserver> navigation_observer_;
    scoped_ptr<ServiceWorkerManager> service_worker_manager_;
    // Shared memory region manager for scripts statically declared in extension
    // manifests. This region is shared between all extensions.
    scoped_ptr<SharedUserScriptMaster> shared_user_script_master_;
    scoped_ptr<RuntimeData> runtime_data_;
    // ExtensionService depends on StateStore, Blacklist and RuntimeData.
    scoped_ptr<ExtensionService> extension_service_;
    scoped_ptr<ManagementPolicy> management_policy_;
    // extension_info_map_ needs to outlive process_manager_.
    scoped_refptr<InfoMap> extension_info_map_;
    scoped_ptr<QuotaService> quota_service_;
    scoped_ptr<AppSorting> app_sorting_;

    // For verifying the contents of extensions read from disk.
    scoped_refptr<ContentVerifier> content_verifier_;

#if defined(OS_CHROMEOS)
    scoped_ptr<chromeos::DeviceLocalAccountManagementPolicyProvider>
        device_local_account_management_policy_provider_;
#endif

    OneShotEvent ready_;
  };

  Profile* profile_;

  Shared* shared_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_
