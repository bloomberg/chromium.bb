// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/extensions/extension_constants.h"

class ExtensionInfoMap;
class ExtensionProcessManager;
class ExtensionService;
class Profile;

namespace extensions {
// Unfortunately, for the ApiResourceManager<> template classes, we don't seem
// to be able to forward-declare because of compilation errors on Windows.
class AlarmManager;
class Blacklist;
class EventRouter;
class Extension;
class ExtensionPrefs;
class ExtensionSystemSharedFactory;
class ExtensionWarningBadgeService;
class ExtensionWarningService;
class LazyBackgroundTaskQueue;
class ManagementPolicy;
class MessageService;
class NavigationObserver;
class RulesRegistryService;
class ShellWindowGeometryCache;
class StandardManagementPolicyProvider;
class StateStore;
class UserScriptMaster;

// The ExtensionSystem manages the creation and destruction of services
// related to extensions. Most objects are shared between normal
// and incognito Profiles, except as called out in comments.
// This interface supports using TestExtensionSystem for TestingProfiles
// that don't want all of the extensions baggage in their tests.
class ExtensionSystem : public ProfileKeyedService {
 public:
  ExtensionSystem();
  virtual ~ExtensionSystem();

  // Returns the instance for the given profile, or NULL if none. This is
  // a convenience wrapper around ExtensionSystemFactory::GetForProfile.
  static ExtensionSystem* Get(Profile* profile);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE {}

  // Initializes extensions machinery.
  // Component extensions are always enabled, external and user extensions
  // are controlled by |extensions_enabled|.
  virtual void InitForRegularProfile(bool extensions_enabled) = 0;

  virtual void InitForOTRProfile() = 0;

  // The ExtensionService is created at startup.
  virtual ExtensionService* extension_service() = 0;

  // The class controlling whether users are permitted to perform certain
  // actions on extensions (install, uninstall, disable, etc.).
  // The ManagementPolicy is created at startup.
  virtual ManagementPolicy* management_policy() = 0;

  // The UserScriptMaster is created at startup.
  virtual UserScriptMaster* user_script_master() = 0;

  // The ExtensionProcessManager is created at startup.
  virtual ExtensionProcessManager* process_manager() = 0;

  // The AlarmManager is created at startup.
  virtual AlarmManager* alarm_manager() = 0;

  // The StateStore is created at startup.
  virtual StateStore* state_store() = 0;

  // The rules store is created at startup.
  virtual StateStore* rules_store() = 0;

  // The extension prefs.
  virtual ExtensionPrefs* extension_prefs() = 0;

  // The ShellWindowGeometryCache is created at startup.
  virtual ShellWindowGeometryCache* shell_window_geometry_cache() = 0;

  // Returns the IO-thread-accessible extension data.
  virtual ExtensionInfoMap* info_map() = 0;

  // The LazyBackgroundTaskQueue is created at startup.
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() = 0;

  // The MessageService is created at startup.
  virtual MessageService* message_service() = 0;

  // The EventRouter is created at startup.
  virtual EventRouter* event_router() = 0;

  // The RulesRegistryService is created at startup.
  virtual RulesRegistryService* rules_registry_service() = 0;

  // The SerialConnection ResourceManager is created at startup.
  virtual ApiResourceManager<SerialConnection>*
  serial_connection_manager() = 0;

  // The Socket ResourceManager is created at startup.
  virtual ApiResourceManager<Socket>*
  socket_manager() = 0;

  // The UsbDeviceResource ResourceManager is created at startup.
  virtual ApiResourceManager<UsbDeviceResource>*
  usb_device_resource_manager() = 0;

  // The ExtensionWarningService is created at startup.
  virtual ExtensionWarningService* warning_service() = 0;

  // The blacklist is created at startup.
  virtual Blacklist* blacklist() = 0;

  // Called by the ExtensionService that lives in this system. Gives the
  // info map a chance to react to the load event before the EXTENSION_LOADED
  // notification has fired. The purpose for handling this event first is to
  // avoid race conditions by making sure URLRequestContexts learn about new
  // extensions before anything else needs them to know.
  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) {}

  // Called by the ExtensionService that lives in this system. Lets the
  // info map clean up its RequestContexts once all the listeners to the
  // EXTENSION_UNLOADED notification have finished running.
  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const extension_misc::UnloadedExtensionReason reason) {}
};

// The ExtensionSystem for ProfileImpl and OffTheRecordProfileImpl.
// Implementation details: non-shared services are owned by
// ExtensionSystemImpl, a ProfileKeyedService with separate incognito
// instances. A private Shared class (also a ProfileKeyedService,
// but with a shared instance for incognito) keeps the common services.
class ExtensionSystemImpl : public ExtensionSystem {
 public:
  explicit ExtensionSystemImpl(Profile* profile);
  virtual ~ExtensionSystemImpl();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE;
  virtual void InitForOTRProfile() OVERRIDE;

  virtual ExtensionService* extension_service() OVERRIDE;  // shared
  virtual ManagementPolicy* management_policy() OVERRIDE;  // shared
  virtual UserScriptMaster* user_script_master() OVERRIDE;  // shared
  virtual ExtensionProcessManager* process_manager() OVERRIDE;
  virtual AlarmManager* alarm_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;  // shared
  virtual StateStore* rules_store() OVERRIDE;  // shared
  virtual ExtensionPrefs* extension_prefs() OVERRIDE;  // shared
  virtual ShellWindowGeometryCache* shell_window_geometry_cache()
      OVERRIDE;  // shared
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue()
      OVERRIDE;  // shared
  virtual ExtensionInfoMap* info_map() OVERRIDE;  // shared
  virtual MessageService* message_service() OVERRIDE;  // shared
  virtual EventRouter* event_router() OVERRIDE;  // shared
  virtual RulesRegistryService* rules_registry_service()
      OVERRIDE;  // shared
  virtual ApiResourceManager<SerialConnection>* serial_connection_manager()
      OVERRIDE;
  virtual ApiResourceManager<Socket>* socket_manager() OVERRIDE;
  virtual ApiResourceManager<UsbDeviceResource>* usb_device_resource_manager()
      OVERRIDE;
  virtual ExtensionWarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;  // shared

  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) OVERRIDE;

  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const extension_misc::UnloadedExtensionReason reason) OVERRIDE;

 private:
  friend class ExtensionSystemSharedFactory;

  // Owns the Extension-related systems that have a single instance
  // shared between normal and incognito profiles.
  class Shared : public ProfileKeyedService {
   public:
    explicit Shared(Profile* profile);
    virtual ~Shared();

    // Initialization takes place in phases.
    virtual void InitPrefs();
    // This must not be called until all the providers have been created.
    void RegisterManagementPolicyProviders();
    void Init(bool extensions_enabled);

    // ProfileKeyedService implementation.
    virtual void Shutdown() OVERRIDE;

    StateStore* state_store();
    StateStore* rules_store();
    ExtensionPrefs* extension_prefs();
    ShellWindowGeometryCache* shell_window_geometry_cache();
    ExtensionService* extension_service();
    ManagementPolicy* management_policy();
    UserScriptMaster* user_script_master();
    Blacklist* blacklist();
    ExtensionInfoMap* info_map();
    LazyBackgroundTaskQueue* lazy_background_task_queue();
    MessageService* message_service();
    EventRouter* event_router();
    ExtensionWarningService* warning_service();

   private:
    Profile* profile_;

    // The services that are shared between normal and incognito profiles.

    scoped_ptr<StateStore> state_store_;
    scoped_ptr<StateStore> rules_store_;
    scoped_ptr<ExtensionPrefs> extension_prefs_;
    // ShellWindowGeometryCache depends on ExtensionPrefs.
    scoped_ptr<ShellWindowGeometryCache> shell_window_geometry_cache_;
    // LazyBackgroundTaskQueue is a dependency of
    // MessageService and EventRouter.
    scoped_ptr<LazyBackgroundTaskQueue> lazy_background_task_queue_;
    scoped_ptr<EventRouter> event_router_;
    scoped_ptr<MessageService> message_service_;
    scoped_ptr<NavigationObserver> navigation_observer_;
    scoped_refptr<UserScriptMaster> user_script_master_;
    // Blacklist depends on ExtensionPrefs.
    scoped_ptr<Blacklist> blacklist_;
    // StandardManagementPolicyProvider depends on ExtensionPrefs and Blacklist.
    scoped_ptr<StandardManagementPolicyProvider>
        standard_management_policy_provider_;
    // ExtensionService depends on ExtensionPrefs, StateStore, and Blacklist.
    scoped_ptr<ExtensionService> extension_service_;
    scoped_ptr<ManagementPolicy> management_policy_;
    // extension_info_map_ needs to outlive extension_process_manager_.
    scoped_refptr<ExtensionInfoMap> extension_info_map_;
    scoped_ptr<ExtensionWarningService> extension_warning_service_;
    scoped_ptr<ExtensionWarningBadgeService> extension_warning_badge_service_;
  };

  Profile* profile_;

  Shared* shared_;

  // |extension_process_manager_| must be destroyed before the Profile's
  // |io_data_|. While |extension_process_manager_| still lives, we handle
  // incoming resource requests from extension processes and those require
  // access to the ResourceContext owned by |io_data_|.
  scoped_ptr<ExtensionProcessManager> extension_process_manager_;
  scoped_ptr<AlarmManager> alarm_manager_;
  scoped_ptr<ApiResourceManager<SerialConnection> > serial_connection_manager_;
  scoped_ptr<ApiResourceManager<Socket> > socket_manager_;
  scoped_ptr<ApiResourceManager<
               UsbDeviceResource> > usb_device_resource_manager_;
  scoped_ptr<RulesRegistryService> rules_registry_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_
