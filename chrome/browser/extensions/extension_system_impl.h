// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_

#include "base/memory/scoped_vector.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

class Profile;

namespace extensions {

class ContentVerifier;
class DeclarativeUserScriptMaster;
class ExtensionSystemSharedFactory;
class ExtensionWarningBadgeService;
class NavigationObserver;
class SharedUserScriptMaster;
class StandardManagementPolicyProvider;
class StateStoreNotificationObserver;

// The ExtensionSystem for ProfileImpl and OffTheRecordProfileImpl.
// Implementation details: non-shared services are owned by
// ExtensionSystemImpl, a KeyedService with separate incognito
// instances. A private Shared class (also a KeyedService,
// but with a shared instance for incognito) keeps the common services.
class ExtensionSystemImpl : public ExtensionSystem {
 public:
  explicit ExtensionSystemImpl(Profile* profile);
  virtual ~ExtensionSystemImpl();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE;

  virtual ExtensionService* extension_service() OVERRIDE;  // shared
  virtual RuntimeData* runtime_data() OVERRIDE;  // shared
  virtual ManagementPolicy* management_policy() OVERRIDE;  // shared
  // shared
  virtual SharedUserScriptMaster* shared_user_script_master() OVERRIDE;
  virtual ProcessManager* process_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;  // shared
  virtual StateStore* rules_store() OVERRIDE;  // shared
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue()
      OVERRIDE;  // shared
  virtual InfoMap* info_map() OVERRIDE; // shared
  virtual EventRouter* event_router() OVERRIDE;  // shared
  virtual ExtensionWarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;  // shared
  virtual ErrorConsole* error_console() OVERRIDE;
  virtual InstallVerifier* install_verifier() OVERRIDE;
  virtual QuotaService* quota_service() OVERRIDE;  // shared

  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) OVERRIDE;

  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) OVERRIDE;

  virtual const OneShotEvent& ready() const OVERRIDE;
  virtual ContentVerifier* content_verifier() OVERRIDE;  // shared
  virtual scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) OVERRIDE;

  virtual DeclarativeUserScriptMaster*
      GetDeclarativeUserScriptMasterByExtension(
          const ExtensionId& extension_id) OVERRIDE;  // shared

 private:
  friend class ExtensionSystemSharedFactory;

  // Owns the Extension-related systems that have a single instance
  // shared between normal and incognito profiles.
  class Shared : public KeyedService {
   public:
    explicit Shared(Profile* profile);
    virtual ~Shared();

    // Initialization takes place in phases.
    virtual void InitPrefs();
    // This must not be called until all the providers have been created.
    void RegisterManagementPolicyProviders();
    void Init(bool extensions_enabled);

    // KeyedService implementation.
    virtual void Shutdown() OVERRIDE;

    StateStore* state_store();
    StateStore* rules_store();
    ExtensionService* extension_service();
    RuntimeData* runtime_data();
    ManagementPolicy* management_policy();
    SharedUserScriptMaster* shared_user_script_master();
    Blacklist* blacklist();
    InfoMap* info_map();
    LazyBackgroundTaskQueue* lazy_background_task_queue();
    EventRouter* event_router();
    ExtensionWarningService* warning_service();
    ErrorConsole* error_console();
    InstallVerifier* install_verifier();
    QuotaService* quota_service();
    const OneShotEvent& ready() const { return ready_; }
    ContentVerifier* content_verifier();

    DeclarativeUserScriptMaster* GetDeclarativeUserScriptMasterByExtension(
        const ExtensionId& extension_id);

   private:
    Profile* profile_;

    // The services that are shared between normal and incognito profiles.

    scoped_ptr<StateStore> state_store_;
    scoped_ptr<StateStoreNotificationObserver>
        state_store_notification_observer_;
    scoped_ptr<StateStore> rules_store_;
    // LazyBackgroundTaskQueue is a dependency of
    // MessageService and EventRouter.
    scoped_ptr<LazyBackgroundTaskQueue> lazy_background_task_queue_;
    scoped_ptr<EventRouter> event_router_;
    scoped_ptr<NavigationObserver> navigation_observer_;
    // Shared memory region manager for scripts statically declared in extension
    // manifests. This region is shared between all extensions.
    scoped_ptr<SharedUserScriptMaster> shared_user_script_master_;
    // Shared memory region manager for programmatically declared scripts, one
    // per extension. Managers are instantiated the first time the declarative
    // API is used by an extension to request content scripts.
    ScopedVector<DeclarativeUserScriptMaster> declarative_user_script_masters_;
    scoped_ptr<Blacklist> blacklist_;
    // StandardManagementPolicyProvider depends on Blacklist.
    scoped_ptr<StandardManagementPolicyProvider>
        standard_management_policy_provider_;
    scoped_ptr<RuntimeData> runtime_data_;
    // ExtensionService depends on StateStore, Blacklist and RuntimeData.
    scoped_ptr<ExtensionService> extension_service_;
    scoped_ptr<ManagementPolicy> management_policy_;
    // extension_info_map_ needs to outlive process_manager_.
    scoped_refptr<InfoMap> extension_info_map_;
    scoped_ptr<ExtensionWarningService> extension_warning_service_;
    scoped_ptr<ExtensionWarningBadgeService> extension_warning_badge_service_;
    scoped_ptr<ErrorConsole> error_console_;
    scoped_ptr<InstallVerifier> install_verifier_;
    scoped_ptr<QuotaService> quota_service_;

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

  // |process_manager_| must be destroyed before the Profile's |io_data_|. While
  // |process_manager_| still lives, we handle incoming resource requests from
  // extension processes and those require access to the ResourceContext owned
  // by |io_data_|.
  scoped_ptr<ProcessManager> process_manager_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_IMPL_H_
