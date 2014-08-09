// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "sync/api/string_ordinal.h"

class ExtensionSyncService;
class GURL;
class Profile;

namespace base {
class CommandLine;
class SequencedTaskRunner;
class Version;
}

namespace content {
class DevToolsAgentHost;
}

namespace extensions {
class ComponentLoader;
class CrxInstaller;
class ExtensionActionStorageManager;
class ExtensionErrorController;
class ExtensionRegistry;
class ExtensionSystem;
class ExtensionUpdater;
class OneShotEvent;
class ExternalInstallManager;
class SharedModuleService;
class UpdateObserver;
}  // namespace extensions

// This is an interface class to encapsulate the dependencies that
// various classes have on ExtensionService. This allows easy mocking.
class ExtensionServiceInterface
    : public base::SupportsWeakPtr<ExtensionServiceInterface> {
 public:
  virtual ~ExtensionServiceInterface() {}

  // DEPRECATED: Use ExtensionRegistry::enabled_extensions() instead.
  //
  // ExtensionRegistry also has the disabled, terminated and blacklisted sets.
  virtual const extensions::ExtensionSet* extensions() const = 0;

  // Gets the object managing the set of pending extensions.
  virtual extensions::PendingExtensionManager* pending_extension_manager() = 0;

  // Installs an update with the contents from |extension_path|. Returns true if
  // the install can be started. Sets |out_crx_installer| to the installer if
  // one was started.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual bool UpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      bool file_ownership_passed,
      extensions::CrxInstaller** out_crx_installer) = 0;

  // DEPRECATED. Use ExtensionRegistry instead.
  //
  // Looks up an extension by its ID.
  //
  // If |include_disabled| is false then this will only include enabled
  // extensions. Use instead:
  //
  //   ExtensionRegistry::enabled_extensions().GetByID(id).
  //
  // If |include_disabled| is true then this will also include disabled and
  // blacklisted extensions (not terminated extensions). Use instead:
  //
  //   ExtensionRegistry::GetExtensionById(
  //         id, ExtensionRegistry::ENABLED |
  //             ExtensionRegistry::DISABLED |
  //             ExtensionRegistry::BLACKLISTED)
  //
  // Or don't, because it's probably not something you ever need to know.
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id,
      bool include_disabled) const = 0;

  // DEPRECATED: Use ExtensionRegistry instead.
  //
  // Looks up an extension by ID, regardless of whether it's enabled,
  // disabled, blacklisted, or terminated. Use instead:
  //
  //   ExtensionRegistry::GetExtensionById(id, ExtensionRegistry::EVERYTHING).
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const = 0;

  // Returns an update for an extension with the specified id, if installation
  // of that update was previously delayed because the extension was in use. If
  // no updates are pending for the extension returns NULL.
  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const = 0;

  // Finishes installation of an update for an extension with the specified id,
  // when installation of that extension was previously delayed because the
  // extension was in use.
  virtual void FinishDelayedInstallation(const std::string& extension_id) = 0;

  // Returns true if the extension with the given |extension_id| is enabled.
  // This will return a valid answer even if the extension is not loaded yet.
  virtual bool IsExtensionEnabled(const std::string& extension_id) const = 0;

  // Go through each extension and unload those that are not allowed to run by
  // management policy providers (ie. network admin and Google-managed
  // blacklist).
  virtual void CheckManagementPolicy() = 0;

  // Safe to call multiple times in a row.
  //
  // TODO(akalin): Remove this method (and others) once we refactor
  // themes sync to not use it directly.
  virtual void CheckForUpdatesSoon() = 0;

  // Adds |extension| to this ExtensionService and notifies observers that the
  // extensions have been loaded.
  virtual void AddExtension(const extensions::Extension* extension) = 0;

  // Check if we have preferences for the component extension and, if not or if
  // the stored version differs, install the extension (without requirements
  // checking) before calling AddExtension.
  virtual void AddComponentExtension(
      const extensions::Extension* extension) = 0;

  // Unload the specified extension.
  virtual void UnloadExtension(
      const std::string& extension_id,
      extensions::UnloadedExtensionInfo::Reason reason) = 0;

  // Remove the specified component extension.
  virtual void RemoveComponentExtension(const std::string& extension_id) = 0;

  // Whether the extension service is ready.
  virtual bool is_ready() = 0;

  // Returns task runner for crx installation file I/O operations.
  virtual base::SequencedTaskRunner* GetFileTaskRunner() = 0;
};

// Manages installed and running Chromium extensions. An instance is shared
// between normal and incognito profiles.
class ExtensionService
    : public ExtensionServiceInterface,
      public extensions::ExternalProviderInterface::VisitorInterface,
      public content::NotificationObserver,
      public extensions::Blacklist::Observer {
 public:
  // Attempts to uninstall an extension from a given ExtensionService. Returns
  // true iff the target extension exists.
  static bool UninstallExtensionHelper(ExtensionService* extensions_service,
                                       const std::string& extension_id,
                                       extensions::UninstallReason reason);

  // Constructor stores pointers to |profile| and |extension_prefs| but
  // ownership remains at caller.
  ExtensionService(Profile* profile,
                   const base::CommandLine* command_line,
                   const base::FilePath& install_directory,
                   extensions::ExtensionPrefs* extension_prefs,
                   extensions::Blacklist* blacklist,
                   bool autoupdate_enabled,
                   bool extensions_enabled,
                   extensions::OneShotEvent* ready);

  virtual ~ExtensionService();

  // ExtensionServiceInterface implementation.
  //
  // NOTE: Many of these methods are DEPRECATED. See the interface for details.
  virtual const extensions::ExtensionSet* extensions() const OVERRIDE;
  virtual extensions::PendingExtensionManager*
      pending_extension_manager() OVERRIDE;
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const OVERRIDE;
  virtual bool UpdateExtension(
      const std::string& id,
      const base::FilePath& extension_path,
      bool file_ownership_passed,
      extensions::CrxInstaller** out_crx_installer) OVERRIDE;
  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const OVERRIDE;
  virtual void UnloadExtension(
      const std::string& extension_id,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void RemoveComponentExtension(const std::string& extension_id)
      OVERRIDE;
  virtual void AddExtension(const extensions::Extension* extension) OVERRIDE;
  virtual void AddComponentExtension(const extensions::Extension* extension)
      OVERRIDE;
  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const OVERRIDE;
  virtual void FinishDelayedInstallation(
     const std::string& extension_id) OVERRIDE;
  virtual void CheckManagementPolicy() OVERRIDE;
  virtual void CheckForUpdatesSoon() OVERRIDE;
  virtual bool is_ready() OVERRIDE;
  virtual base::SequencedTaskRunner* GetFileTaskRunner() OVERRIDE;

  // ExternalProvider::Visitor implementation.
  // Exposed for testing.
  virtual bool OnExternalExtensionFileFound(
      const std::string& id,
      const base::Version* version,
      const base::FilePath& path,
      extensions::Manifest::Location location,
      int creation_flags,
      bool mark_acknowledged) OVERRIDE;
  virtual bool OnExternalExtensionUpdateUrlFound(
      const std::string& id,
      const std::string& install_parameter,
      const GURL& update_url,
      extensions::Manifest::Location location,
      int creation_flags,
      bool mark_acknowledged) OVERRIDE;
  virtual void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) OVERRIDE;

  // Initialize and start all installed extensions.
  void Init();

  // Called when the associated Profile is going to be destroyed.
  void Shutdown();

  // Reloads the specified extension, sending the onLaunched() event to it if it
  // currently has any window showing.
  // Allows noisy failures.
  void ReloadExtension(const std::string& extension_id);

  // Suppresses noisy failures.
  void ReloadExtensionWithQuietFailure(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |reason| lets the caller specify why the
  // extension is uninstalled.
  //
  // If the return value is true, |deletion_done_callback| is invoked when data
  // deletion is done or at least is scheduled.
  virtual bool UninstallExtension(const std::string& extension_id,
                                  extensions::UninstallReason reason,
                                  const base::Closure& deletion_done_callback,
                                  base::string16* error);

  // Enables the extension.  If the extension is already enabled, does
  // nothing.
  virtual void EnableExtension(const std::string& extension_id);

  // Disables the extension.  If the extension is already disabled, or
  // cannot be disabled, does nothing.
  virtual void DisableExtension(
      const std::string& extension_id,
      extensions::Extension::DisableReason disable_reason);

  // Disable non-default and non-managed extensions with ids not in
  // |except_ids|. Default extensions are those from the Web Store with
  // |was_installed_by_default| flag.
  void DisableUserExtensions(const std::vector<std::string>& except_ids);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extension|'s manifest and re-enables the
  // extension.
  void GrantPermissionsAndEnableExtension(
      const extensions::Extension* extension);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extensions|'s manifest.
  void GrantPermissions(const extensions::Extension* extension);

  // Check for updates (or potentially new extensions from external providers)
  void CheckForExternalUpdates();

  // Called when the initial extensions load has completed.
  virtual void OnLoadedInstalledExtensions();

  // Informs the service that an extension's files are in place for loading.
  //
  // |extension|     the extension
  // |page_ordinal|  the location of the extension in the app launcher
  // |install_flags| a bitmask of extensions::InstallFlags
  void OnExtensionInstalled(const extensions::Extension* extension,
                            const syncer::StringOrdinal& page_ordinal,
                            int install_flags);
  void OnExtensionInstalled(const extensions::Extension* extension,
                            const syncer::StringOrdinal& page_ordinal) {
    OnExtensionInstalled(extension,
                         page_ordinal,
                         static_cast<int>(extensions::kInstallFlagNone));
  }

  // Checks for delayed installation for all pending installs.
  void MaybeFinishDelayedInstallations();

  // Promotes an ephemeral app to a regular installed app. Ephemeral apps
  // are already installed in extension system (albiet transiently) and only
  // need to be exposed in the UI. Set |is_from_sync| to true if the
  // install was initiated via sync.
  void PromoteEphemeralApp(
      const extensions::Extension* extension, bool is_from_sync);

  // ExtensionHost of background page calls this method right after its render
  // view has been created.
  void DidCreateRenderViewForBackgroundPage(extensions::ExtensionHost* host);

  // Changes sequenced task runner for crx installation tasks to |task_runner|.
  void SetFileTaskRunnerForTesting(base::SequencedTaskRunner* task_runner);

  // Postpone installations so that we don't have to worry about race
  // conditions.
  void OnGarbageCollectIsolatedStorageStart();

  // Restart any extension installs which were delayed for isolated storage
  // garbage collection.
  void OnGarbageCollectIsolatedStorageFinished();

  // Record a histogram using the PermissionMessage enum values for each
  // permission in |e|.
  // NOTE: If this is ever called with high frequency, the implementation may
  // need to be made more efficient.
  static void RecordPermissionMessagesHistogram(
      const extensions::Extension* extension, const char* histogram);

  // Unloads the given extension and mark the extension as terminated. This
  // doesn't notify the user that the extension was terminated, if such a
  // notification is desired the calling code is responsible for doing that.
  void TerminateExtension(const std::string& extension_id);

  // Adds/Removes update observers.
  void AddUpdateObserver(extensions::UpdateObserver* observer);
  void RemoveUpdateObserver(extensions::UpdateObserver* observer);

  //////////////////////////////////////////////////////////////////////////////
  // Simple Accessors

  // Returns a WeakPtr to the ExtensionService.
  base::WeakPtr<ExtensionService> AsWeakPtr() { return base::AsWeakPtr(this); }

  // Returns profile_ as a BrowserContext.
  content::BrowserContext* GetBrowserContext() const;

  bool extensions_enabled() const { return extensions_enabled_; }
  void set_extensions_enabled(bool enabled) { extensions_enabled_ = enabled; }

  const base::FilePath& install_directory() const { return install_directory_; }

  const extensions::ExtensionSet* delayed_installs() const {
    return &delayed_installs_;
  }

  bool show_extensions_prompts() const { return show_extensions_prompts_; }
  void set_show_extensions_prompts(bool show_extensions_prompts) {
    show_extensions_prompts_ = show_extensions_prompts;
  }

  Profile* profile() { return profile_; }

  void set_extension_sync_service(
      ExtensionSyncService* extension_sync_service) {
    extension_sync_service_ = extension_sync_service;
  }

  // Note that this may return NULL if autoupdate is not turned on.
  extensions::ExtensionUpdater* updater() { return updater_.get(); }

  extensions::ComponentLoader* component_loader() {
    return component_loader_.get();
  }

  bool browser_terminating() const { return browser_terminating_; }

  extensions::SharedModuleService* shared_module_service() {
    return shared_module_service_.get();
  }

  extensions::ExternalInstallManager* external_install_manager() {
    return external_install_manager_.get();
  }

  //////////////////////////////////////////////////////////////////////////////
  // For Testing

  // Unload all extensions. Does not send notifications.
  void UnloadAllExtensionsForTest();

  // Reloads all extensions. Does not notify that extensions are ready.
  void ReloadExtensionsForTest();

  // Clear all ExternalProviders.
  void ClearProvidersForTesting();

  // Adds an ExternalProviderInterface for the service to use during testing.
  // Takes ownership of |test_provider|.
  void AddProviderForTesting(
      extensions::ExternalProviderInterface* test_provider);

  // Simulate an extension being blacklisted for tests.
  void BlacklistExtensionForTest(const std::string& extension_id);

#if defined(UNIT_TEST)
  void TrackTerminatedExtensionForTest(const extensions::Extension* extension) {
    TrackTerminatedExtension(extension);
  }

  void FinishInstallationForTest(const extensions::Extension* extension) {
    FinishInstallation(extension, false /* not ephemeral */);
  }
#endif

  void set_browser_terminating_for_test(bool value) {
    browser_terminating_ = value;
  }

  // By default ExtensionService will wait with installing an updated extension
  // until the extension is idle. Tests might not like this behavior, so you can
  // disable it with this method.
  void set_install_updates_when_idle_for_test(bool value) {
    install_updates_when_idle_ = value;
  }

  // Set a callback to be called when all external providers are ready and their
  // extensions have been installed.
  void set_external_updates_finished_callback_for_test(
      const base::Closure& callback) {
    external_updates_finished_callback_ = callback;
  }


 private:
  // Reloads the specified extension, sending the onLaunched() event to it if it
  // currently has any window showing. |be_noisy| determines whether noisy
  // failures are allowed for unpacked extension installs.
  void ReloadExtensionImpl(const std::string& extension_id, bool be_noisy);

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // extensions::Blacklist::Observer implementation.
  virtual void OnBlacklistUpdated() OVERRIDE;

  // Similar to FinishInstallation, but first checks if there still is an update
  // pending for the extension, and makes sure the extension is still idle.
  void MaybeFinishDelayedInstallation(const std::string& extension_id);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, uninstall it.
  void CheckExternalUninstall(const std::string& id);

  // Populates greylist_.
  void LoadGreylistFromPrefs();

  // Signals *ready_ and sends a notification to the listeners.
  void SetReadyAndNotifyListeners();

  // Returns true if all the external extension providers are ready.
  bool AreAllExternalProvidersReady() const;

  // Called once all external providers are ready. Checks for unclaimed
  // external extensions.
  void OnAllExternalProvidersReady();

  // Return true if the sync type of |extension| matches |type|.
  void OnExtensionInstallPrefChanged();

  // Adds the given extension to the list of terminated extensions if
  // it is not already there and unloads it.
  void TrackTerminatedExtension(const extensions::Extension* extension);

  // Removes the extension with the given id from the list of
  // terminated extensions if it is there.
  void UntrackTerminatedExtension(const std::string& id);

  // Update preferences for a new or updated extension; notify observers that
  // the extension is installed, e.g., to update event handlers on background
  // pages; and perform other extension install tasks before calling
  // AddExtension.
  // |install_flags| is a bitmask of extensions::InstallFlags.
  void AddNewOrUpdatedExtension(const extensions::Extension* extension,
                                extensions::Extension::State initial_state,
                                int install_flags,
                                const syncer::StringOrdinal& page_ordinal,
                                const std::string& install_parameter);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const extensions::Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason);

  // Common helper to finish installing the given extension. |was_ephemeral|
  // should be true if the extension was previously installed and ephemeral.
  void FinishInstallation(const extensions::Extension* extension,
                          bool was_ephemeral);

  // Disables the extension if the privilege level has increased
  // (e.g., due to an upgrade).
  void CheckPermissionsIncrease(const extensions::Extension* extension,
                                bool is_extension_installed);

  // Helper that updates the active extension list used for crash reporting.
  void UpdateActiveExtensionsInCrashReporter();

  // Helper to determine whether we should initially enable an installed
  // (or upgraded) extension.
  bool ShouldEnableOnInstall(const extensions::Extension* extension);

  // Helper to determine if updating an extensions should proceed immediately,
  // or if we should delay the update until further notice.
  bool ShouldDelayExtensionUpdate(const std::string& extension_id,
                                  bool install_immediately) const;

  // Manages the blacklisted extensions, intended as callback from
  // Blacklist::GetBlacklistedIDs.
  void ManageBlacklist(
      const extensions::Blacklist::BlacklistStateMap& blacklisted_ids);

  // Add extensions in |blocked| to blacklisted_extensions, remove extensions
  // that are neither in |blocked|, nor in |unchanged|.
  void UpdateBlockedExtensions(const extensions::ExtensionIdSet& blocked,
                               const extensions::ExtensionIdSet& unchanged);

  void UpdateGreylistedExtensions(
      const extensions::ExtensionIdSet& greylist,
      const extensions::ExtensionIdSet& unchanged,
      const extensions::Blacklist::BlacklistStateMap& state_map);

  // Used only by test code.
  void UnloadAllExtensionsInternal();

  // Disable apps & extensions now to stop them from running after a profile
  // has been conceptually deleted. Don't wait for full browser shutdown and
  // the actual profile objects to be destroyed.
  void OnProfileDestructionStarted();

  // Called on file task runner thread to uninstall extension.
  static void UninstallExtensionOnFileThread(
      const std::string& id,
      Profile* profile,
      const base::FilePath& install_dir,
      const base::FilePath& extension_path);

  // The normal profile associated with this ExtensionService.
  Profile* profile_;

  // The ExtensionSystem for the profile above.
  extensions::ExtensionSystem* system_;

  // Preferences for the owning profile.
  extensions::ExtensionPrefs* extension_prefs_;

  // Blacklist for the owning profile.
  extensions::Blacklist* blacklist_;

  // The ExtensionSyncService that is used by this ExtensionService.
  ExtensionSyncService* extension_sync_service_;

  // Sets of enabled/disabled/terminated/blacklisted extensions. Not owned.
  extensions::ExtensionRegistry* registry_;

  // Set of greylisted extensions. These extensions are disabled if they are
  // already installed in Chromium at the time when they are added to
  // the greylist. Unlike blacklisted extensions, greylisted ones are visible
  // to the user and if user re-enables such an extension, they remain enabled.
  //
  // These extensions should appear in registry_.
  extensions::ExtensionSet greylist_;

  // The list of extension installs delayed for various reasons.  The reason
  // for delayed install is stored in ExtensionPrefs. These are not part of
  // ExtensionRegistry because they are not yet installed.
  extensions::ExtensionSet delayed_installs_;

  // Hold the set of pending extensions.
  extensions::PendingExtensionManager pending_extension_manager_;

  // The full path to the directory where extensions are installed.
  base::FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_;

  // Whether to notify users when they attempt to install an extension.
  bool show_extensions_prompts_;

  // Whether to delay installing of extension updates until the extension is
  // idle.
  bool install_updates_when_idle_;

  // Signaled when all extensions are loaded.
  extensions::OneShotEvent* const ready_;

  // Our extension updater, if updates are turned on.
  scoped_ptr<extensions::ExtensionUpdater> updater_;

  // Map unloaded extensions' ids to their paths. When a temporarily loaded
  // extension is unloaded, we lose the information about it and don't have
  // any in the extension preferences file.
  typedef std::map<std::string, base::FilePath> UnloadedExtensionPathMap;
  UnloadedExtensionPathMap unloaded_extension_paths_;

  // Map of DevToolsAgentHost instances that are detached,
  // waiting for an extension to be reloaded.
  typedef std::map<std::string, scoped_refptr<content::DevToolsAgentHost> >
      OrphanedDevTools;
  OrphanedDevTools orphaned_dev_tools_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Keeps track of loading and unloading component extensions.
  scoped_ptr<extensions::ComponentLoader> component_loader_;

  // A collection of external extension providers.  Each provider reads
  // a source of external extension information.  Examples include the
  // windows registry and external_extensions.json.
  extensions::ProviderCollection external_extension_providers_;

  // Set to true by OnExternalExtensionUpdateUrlFound() when an external
  // extension URL is found, and by CheckForUpdatesSoon() when an update check
  // has to wait for the external providers.  Used in
  // OnAllExternalProvidersReady() to determine if an update check is needed to
  // install pending extensions.
  bool update_once_all_providers_are_ready_;

  // A callback to be called when all external providers are ready and their
  // extensions have been installed. Normally this is a null callback, but
  // is used in external provider related tests.
  base::Closure external_updates_finished_callback_;

  // Set when the browser is terminating. Prevents us from installing or
  // updating additional extensions and allows in-progress installations to
  // decide to abort.
  bool browser_terminating_;

  // Set to true to delay all new extension installations. Acts as a lock to
  // allow background processing of garbage collection of on-disk state without
  // needing to worry about race conditions caused by extension installation and
  // reinstallation.
  bool installs_delayed_for_gc_;

  // Set to true if this is the first time this ExtensionService has run.
  // Used for specially handling external extensions that are installed the
  // first time.
  bool is_first_run_;

  // Store the ids of reloading extensions. We use this to re-enable extensions
  // which were disabled for a reload.
  std::set<std::string> reloading_extensions_;

  // A set of the extension ids currently being terminated. We use this to
  // avoid trying to unload the same extension twice.
  std::set<std::string> extensions_being_terminated_;

  // The controller for the UI that alerts the user about any blacklisted
  // extensions.
  scoped_ptr<extensions::ExtensionErrorController> error_controller_;

  // The manager for extensions that were externally installed that is
  // responsible for prompting the user about suspicious extensions.
  scoped_ptr<extensions::ExternalInstallManager> external_install_manager_;

  // Sequenced task runner for extension related file operations.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<extensions::ExtensionActionStorageManager>
      extension_action_storage_manager_;
  scoped_ptr<extensions::ManagementPolicy::Provider>
      shared_module_policy_provider_;

  // The SharedModuleService used to check for import dependencies.
  scoped_ptr<extensions::SharedModuleService> shared_module_service_;

  ObserverList<extensions::UpdateObserver, true> update_observers_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           DestroyingProfileClearsExtensions);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, SetUnsetBlacklistInPrefs);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           BlacklistedExtensionWillNotInstall);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           UnloadBlacklistedExtensionPolicy);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           WillNotLoadBlacklistedExtensionsFromDirectory);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, ReloadBlacklistedExtension);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, BlacklistedInPrefsFromStartup);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           GreylistedExtensionDisabled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           GreylistDontEnableManuallyDisabled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           GreylistUnknownDontChange);

  DISALLOW_COPY_AND_ASSIGN(ExtensionService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
