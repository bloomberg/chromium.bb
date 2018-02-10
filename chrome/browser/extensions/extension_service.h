// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/install_gate.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/upgrade_observer.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/features/features.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

class HostContentSettingsMap;
class Profile;

namespace base {
class CommandLine;
}

namespace extensions {
class AppDataMigrator;
class ComponentLoader;
class CrxInstaller;
class ExtensionActionStorageManager;
class ExtensionErrorController;
class ExtensionRegistry;
class ExtensionSystem;
class ExtensionUpdater;
class ExternalInstallManager;
class OneShotEvent;
class SharedModuleService;
class UpdateObserver;
}  // namespace extensions

// This is an interface class to encapsulate the dependencies that
// various classes have on ExtensionService. This allows easy mocking.
class ExtensionServiceInterface
    : public base::SupportsWeakPtr<ExtensionServiceInterface> {
 public:
  virtual ~ExtensionServiceInterface() {}

  // Gets the object managing the set of pending extensions.
  virtual extensions::PendingExtensionManager* pending_extension_manager() = 0;

  // Installs an update with the contents from |extension_path|. Returns true if
  // the install can be started. Sets |out_crx_installer| to the installer if
  // one was started.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual bool UpdateExtension(
      const extensions::CRXFileInfo& file,
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
  // ExtensionRegistry::GetInstalledExtension(id).
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const = 0;

  // Returns an update for an extension with the specified id, if installation
  // of that update was previously delayed because the extension was in use. If
  // no updates are pending for the extension returns NULL.
  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const = 0;

  // Attempts finishing installation of an update for an extension with the
  // specified id, when installation of that extension was previously delayed.
  // |install_immediately| - Whether the extension should be installed if it's
  //     currently in use.
  // Returns whether the extension installation was finished.
  virtual bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                                bool install_immediately) = 0;

  // Returns true if the extension with the given |extension_id| is enabled.
  // This will only return a valid answer for installed extensions (regardless
  // of whether it is currently loaded or not).  Loaded extensions return true
  // if they are currently loaded or terminated.  Unloaded extensions will
  // return true if they are not blocked, disabled, blacklisted or uninstalled
  // (for external extensions).
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
  // extension has been loaded.
  virtual void AddExtension(const extensions::Extension* extension) = 0;

  // Check if we have preferences for the component extension and, if not or if
  // the stored version differs, install the extension (without requirements
  // checking) before calling AddExtension.
  virtual void AddComponentExtension(
      const extensions::Extension* extension) = 0;

  // Unload the specified extension.
  virtual void UnloadExtension(const std::string& extension_id,
                               extensions::UnloadedExtensionReason reason) = 0;

  // Remove the specified component extension.
  virtual void RemoveComponentExtension(const std::string& extension_id) = 0;

  // Whether the extension service is ready.
  virtual bool is_ready() = 0;

};

// Manages installed and running Chromium extensions. An instance is shared
// between normal and incognito profiles.
class ExtensionService
    : public ExtensionServiceInterface,
      public extensions::ExternalProviderInterface::VisitorInterface,
      public content::NotificationObserver,
      public extensions::Blacklist::Observer,
      public extensions::ExtensionManagement::Observer,
      public UpgradeObserver,
      public extensions::ExtensionRegistrar::Delegate {
 public:
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

  ~ExtensionService() override;

  // ExtensionServiceInterface implementation.
  //
  // NOTE: Many of these methods are DEPRECATED. See the interface for details.
  extensions::PendingExtensionManager* pending_extension_manager() override;
  const extensions::Extension* GetExtensionById(
      const std::string& id,
      bool include_disabled) const override;
  const extensions::Extension* GetInstalledExtension(
      const std::string& id) const override;
  bool UpdateExtension(const extensions::CRXFileInfo& file,
                       bool file_ownership_passed,
                       extensions::CrxInstaller** out_crx_installer) override;
  bool IsExtensionEnabled(const std::string& extension_id) const override;
  void UnloadExtension(const std::string& extension_id,
                       extensions::UnloadedExtensionReason reason) override;
  void RemoveComponentExtension(const std::string& extension_id) override;
  void AddExtension(const extensions::Extension* extension) override;
  void AddComponentExtension(const extensions::Extension* extension) override;
  const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;
  void CheckManagementPolicy() override;
  void CheckForUpdatesSoon() override;
  bool is_ready() override;

  // ExternalProvider::VisitorInterface implementation.
  // Exposed for testing.
  bool OnExternalExtensionFileFound(
      const extensions::ExternalInstallInfoFile& info) override;
  bool OnExternalExtensionUpdateUrlFound(
      const extensions::ExternalInstallInfoUpdateUrl& info,
      bool is_initial_load) override;
  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override;
  void OnExternalProviderUpdateComplete(
      const extensions::ExternalProviderInterface* provider,
      const std::vector<extensions::ExternalInstallInfoUpdateUrl>&
          external_update_url_extensions,
      const std::vector<extensions::ExternalInstallInfoFile>&
          external_file_extensions,
      const std::set<std::string>& removed_extensions) override;

  // ExtensionManagement::Observer implementation:
  void OnExtensionManagementSettingsChanged() override;

  // Initialize and start all installed extensions.
  void Init();

  // Called when the associated Profile is going to be destroyed.
  void Shutdown();

  // Reloads the specified extension, sending the onLaunched() event to it if it
  // currently has any window showing.
  // Allows noisy failures.
  // NOTE: Reloading an extension can invalidate |extension_id| and Extension
  // pointers for the given extension. Consider making a copy of |extension_id|
  // first and retrieving a new Extension pointer afterwards.
  void ReloadExtension(const std::string& extension_id);

  // Suppresses noisy failures.
  void ReloadExtensionWithQuietFailure(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |reason| lets the caller specify why the
  // extension is uninstalled.
  // Note: this method synchronously removes the extension from the
  // set of installed extensions stored in the ExtensionRegistry, but will
  // asynchronously remove site-related data and the files stored on disk.
  // Returns true if an uninstall was successfully triggered; this can fail if
  // the extension cannot be uninstalled (such as a policy force-installed
  // extension).
  bool UninstallExtension(const std::string& extension_id,
                          extensions::UninstallReason reason,
                          base::string16* error);

  // Enables the extension.  If the extension is already enabled, does
  // nothing.
  void EnableExtension(const std::string& extension_id);

  // Disables the extension. If the extension is already disabled, just adds
  // the |disable_reasons| (a bitmask of disable_reason::DisableReason - there
  // can be multiple DisableReasons e.g. when an extension comes in disabled
  // from Sync). If the extension cannot be disabled (due to policy), does
  // nothing.
  void DisableExtension(const std::string& extension_id, int disable_reasons);

  // Disable non-default and non-managed extensions with ids not in
  // |except_ids|. Default extensions are those from the Web Store with
  // |was_installed_by_default| flag.
  void DisableUserExtensionsExcept(const std::vector<std::string>& except_ids);

  // Puts all extensions in a blocked state: Unloading every extension, and
  // preventing them from ever loading until UnblockAllExtensions is called.
  // This state is stored in preferences, so persists until Chrome restarts.
  //
  // Component, external component and whitelisted policy installed extensions
  // are exempt from being Blocked (see CanBlockExtension in .cc file).
  void BlockAllExtensions();

  // All blocked extensions are reverted to their previous state, and are
  // reloaded. Newly added extensions are no longer automatically blocked.
  void UnblockAllExtensions();

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

  // Informs the service that an extension's files are in place for loading.
  //
  // |extension|            the extension
  // |page_ordinal|         the location of the extension in the app launcher
  // |install_flags|        a bitmask of extensions::InstallFlags
  // |dnr_ruleset_checksum| Checksum of the indexed ruleset for the Declarative
  //                        Net Request API.
  void OnExtensionInstalled(
      const extensions::Extension* extension,
      const syncer::StringOrdinal& page_ordinal,
      int install_flags,
      const base::Optional<int>& dnr_ruleset_checksum = base::nullopt);
  void OnExtensionInstalled(const extensions::Extension* extension,
                            const syncer::StringOrdinal& page_ordinal) {
    OnExtensionInstalled(extension,
                         page_ordinal,
                         static_cast<int>(extensions::kInstallFlagNone));
  }

  // Checks for delayed installation for all pending installs.
  void MaybeFinishDelayedInstallations();

  // ExtensionHost of background page calls this method right after its render
  // view has been created.
  void DidCreateRenderViewForBackgroundPage(extensions::ExtensionHost* host);

  // Record a histogram using the PermissionMessage enum values for each
  // permission in |e|.
  // NOTE: If this is ever called with high frequency, the implementation may
  // need to be made more efficient.
  static void RecordPermissionMessagesHistogram(
      const extensions::Extension* extension, const char* histogram);

  // Unloads the given extension and marks the extension as terminated. This
  // doesn't notify the user that the extension was terminated, if such a
  // notification is desired the calling code is responsible for doing that.
  void TerminateExtension(const std::string& extension_id);

  // Register self and content settings API with the specified map.
  static void RegisterContentSettings(
      HostContentSettingsMap* host_content_settings_map,
      Profile* profile);

  // Adds/Removes update observers.
  void AddUpdateObserver(extensions::UpdateObserver* observer);
  void RemoveUpdateObserver(extensions::UpdateObserver* observer);

  // Register/unregister an InstallGate with the service.
  void RegisterInstallGate(extensions::ExtensionPrefs::DelayReason reason,
                           extensions::InstallGate* install_delayer);
  void UnregisterInstallGate(extensions::InstallGate* install_delayer);

  //////////////////////////////////////////////////////////////////////////////
  // Simple Accessors

  // Returns a WeakPtr to the ExtensionService.
  base::WeakPtr<ExtensionService> AsWeakPtr() { return base::AsWeakPtr(this); }

  // Returns profile_ as a BrowserContext.
  content::BrowserContext* GetBrowserContext() const;

  bool extensions_enabled() const { return extensions_enabled_; }

  const base::FilePath& install_directory() const { return install_directory_; }

  const extensions::ExtensionSet* delayed_installs() const {
    return &delayed_installs_;
  }

  Profile* profile() { return profile_; }

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

  // Enable Zip Unpacker Extension component extensions for unit test.
  void EnableZipUnpackerExtensionForTest();

  // Clear all ExternalProviders.
  void ClearProvidersForTesting();

  // Adds an ExternalProviderInterface for the service to use during testing.
  void AddProviderForTesting(
      std::unique_ptr<extensions::ExternalProviderInterface> test_provider);

  // Simulate an extension being blacklisted for tests.
  void BlacklistExtensionForTest(const std::string& extension_id);

#if defined(UNIT_TEST)
  void FinishInstallationForTest(const extensions::Extension* extension) {
    FinishInstallation(extension);
  }

  void UninstallMigratedExtensionsForTest() { UninstallMigratedExtensions(); }
#endif

  void set_browser_terminating_for_test(bool value) {
    browser_terminating_ = value;
  }

  // Set a callback to be called when all external providers are ready and their
  // extensions have been installed.
  void set_external_updates_finished_callback_for_test(
      const base::Closure& callback) {
    external_updates_finished_callback_ = callback;
  }

  void set_external_updates_disabled_for_test(bool value) {
    external_updates_disabled_for_test_ = value;
  }

 private:
  // Loads extensions specified via a command line flag/switch.
  void LoadExtensionsFromCommandLineFlag(const char* switch_name);

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // extensions::Blacklist::Observer implementation.
  void OnBlacklistUpdated() override;

  // UpgradeObserver implementation.
  void OnUpgradeRecommended() override;

  // extensions::ExtensionRegistrar::Delegate implementation.
  void PreAddExtension(const extensions::Extension* extension,
                       const extensions::Extension* old_extension) override;
  void PostActivateExtension(
      scoped_refptr<const extensions::Extension> extension) override;
  void PostDeactivateExtension(
      scoped_refptr<const extensions::Extension> extension) override;
  void LoadExtensionForReload(const extensions::ExtensionId& extension_id,
                              const base::FilePath& path,
                              extensions::ExtensionRegistrar::LoadErrorBehavior
                                  load_error_behavior) override;
  bool CanEnableExtension(const extensions::Extension* extension) override;
  bool CanDisableExtension(const extensions::Extension* extension) override;
  bool ShouldBlockExtension(const extensions::Extension* extension) override;

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, uninstall it.
  void CheckExternalUninstall(const std::string& id);

  // Attempt to enable Zip Unpacker component extension if it is disabled.
  // This function doesn't override MUST_REAMIN_DISABLED management policy.
  // Component extensions are managed and cannot be disabled by user, however,
  // there are some cases having a disabled component extension in profile.
  void EnableZipUnpackerExtension();

  // Attempt to enable all disabled extensions which the only disabled reason is
  // reloading.
  void EnabledReloadableExtensions();

  // Finish install (if possible) of extensions that were still delayed while
  // the browser was shut down.
  void MaybeFinishShutdownDelayed();

  // Populates greylist_.
  void LoadGreylistFromPrefs();

  // Signals *ready_ and sends a notification to the listeners.
  void SetReadyAndNotifyListeners();

  // Returns true if all the external extension providers are ready.
  bool AreAllExternalProvidersReady() const;

  // Called once all external providers are ready. Checks for unclaimed
  // external extensions.
  void OnAllExternalProvidersReady();

  // Update preferences for a new or updated extension; notify observers that
  // the extension is installed, e.g., to update event handlers on background
  // pages; and perform other extension install tasks before calling
  // AddExtension.
  // |install_flags| is a bitmask of extensions::InstallFlags.
  void AddNewOrUpdatedExtension(
      const extensions::Extension* extension,
      extensions::Extension::State initial_state,
      int install_flags,
      const syncer::StringOrdinal& page_ordinal,
      const std::string& install_parameter,
      const base::Optional<int>& dnr_ruleset_checksum);

  // Common helper to finish installing the given extension.
  void FinishInstallation(const extensions::Extension* extension);

  // Disables the extension if the privilege level has increased
  // (e.g., due to an upgrade).
  void CheckPermissionsIncrease(const extensions::Extension* extension,
                                bool is_extension_loaded);

  // Helper that updates the active extension list used for crash reporting.
  void UpdateActiveExtensionsInCrashReporter();

  // Helper to get the disable reasons for an installed (or upgraded) extension.
  // A return value of disable_reason::DISABLE_NONE indicates that we should
  // enable this extension initially.
  int GetDisableReasonsOnInstalled(const extensions::Extension* extension);

  // Helper method to determine if an extension can be blocked.
  bool CanBlockExtension(const extensions::Extension* extension) const;

  // Helper to determine if installing an extensions should proceed immediately,
  // or if we should delay the install until further notice, or if the install
  // should be aborted. A pending install is delayed or aborted when any of the
  // delayers say so and only proceeds when all delayers return INSTALL.
  // |extension| is the extension to be installed. |install_immediately| is the
  // install flag set with the install. |reason| is the reason associated with
  // the install delayer that wants to defer or abort the install.
  extensions::InstallGate::Action ShouldDelayExtensionInstall(
      const extensions::Extension* extension,
      bool install_immediately,
      extensions::ExtensionPrefs::DelayReason* reason) const;

  // Manages the blacklisted extensions, intended as callback from
  // Blacklist::GetBlacklistedIDs.
  void ManageBlacklist(
      const extensions::Blacklist::BlacklistStateMap& blacklisted_ids);

  // Add extensions in |blacklisted| to blacklisted_extensions, remove
  // extensions that are neither in |blacklisted|, nor in |unchanged|.
  void UpdateBlacklistedExtensions(
      const extensions::ExtensionIdSet& to_blacklist,
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

  // Called when the initial extensions load has completed.
  void OnInstalledExtensionsLoaded();

  // Uninstall extensions that have been migrated to component extensions.
  void UninstallMigratedExtensions();

  const base::CommandLine* command_line_ = nullptr;

  // The normal profile associated with this ExtensionService.
  Profile* profile_ = nullptr;

  // The ExtensionSystem for the profile above.
  extensions::ExtensionSystem* system_ = nullptr;

  // Preferences for the owning profile.
  extensions::ExtensionPrefs* extension_prefs_ = nullptr;

  // Blacklist for the owning profile.
  extensions::Blacklist* blacklist_ = nullptr;

  // Sets of enabled/disabled/terminated/blacklisted extensions. Not owned.
  extensions::ExtensionRegistry* registry_ = nullptr;

  // Set of greylisted extensions. These extensions are disabled if they are
  // already installed in Chromium at the time when they are added to
  // the greylist. Unlike blacklisted extensions, greylisted ones are visible
  // to the user and if user re-enables such an extension, they remain enabled.
  //
  // These extensions should appear in registry_.
  extensions::ExtensionSet greylist_;

  // Set of whitelisted enabled extensions loaded from the
  // --disable-extensions-except command line flag.
  std::set<std::string> disable_flag_exempted_extensions_;

  // The list of extension installs delayed for various reasons.  The reason
  // for delayed install is stored in ExtensionPrefs. These are not part of
  // ExtensionRegistry because they are not yet installed.
  extensions::ExtensionSet delayed_installs_;

  // Hold the set of pending extensions.
  extensions::PendingExtensionManager pending_extension_manager_;

  // The full path to the directory where extensions are installed.
  base::FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_ = true;

  // Signaled when all extensions are loaded.
  extensions::OneShotEvent* const ready_;

  // Our extension updater, if updates are turned on.
  std::unique_ptr<extensions::ExtensionUpdater> updater_;

  content::NotificationRegistrar registrar_;

  // Keeps track of loading and unloading component extensions.
  std::unique_ptr<extensions::ComponentLoader> component_loader_;

  // A collection of external extension providers.  Each provider reads
  // a source of external extension information.  Examples include the
  // windows registry and external_extensions.json.
  extensions::ProviderCollection external_extension_providers_;

  // Set to true by OnExternalExtensionUpdateUrlFound() when an external
  // extension URL is found, and by CheckForUpdatesSoon() when an update check
  // has to wait for the external providers.  Used in
  // OnAllExternalProvidersReady() to determine if an update check is needed to
  // install pending extensions.
  bool update_once_all_providers_are_ready_ = false;

  // A callback to be called when all external providers are ready and their
  // extensions have been installed. Normally this is a null callback, but
  // is used in external provider related tests.
  base::Closure external_updates_finished_callback_;

  // Set when the browser is terminating. Prevents us from installing or
  // updating additional extensions and allows in-progress installations to
  // decide to abort.
  bool browser_terminating_ = false;

  // If set, call to CheckForExternalUpdates() will bail out.
  bool external_updates_disabled_for_test_ = false;

  // Set to true if this is the first time this ExtensionService has run.
  // Used for specially handling external extensions that are installed the
  // first time.
  bool is_first_run_ = false;

  // Set to true if extensions are all to be blocked.
  bool block_extensions_ = false;

  // The controller for the UI that alerts the user about any blacklisted
  // extensions.
  std::unique_ptr<extensions::ExtensionErrorController> error_controller_;

  // The manager for extensions that were externally installed that is
  // responsible for prompting the user about suspicious extensions.
  std::unique_ptr<extensions::ExternalInstallManager> external_install_manager_;

  std::unique_ptr<extensions::ExtensionActionStorageManager>
      extension_action_storage_manager_;

  // The SharedModuleService used to check for import dependencies.
  std::unique_ptr<extensions::SharedModuleService> shared_module_service_;

  base::ObserverList<extensions::UpdateObserver, true> update_observers_;

  // Migrates app data when upgrading a legacy packaged app to a platform app
  std::unique_ptr<extensions::AppDataMigrator> app_data_migrator_;

  // Helper to register and unregister extensions.
  extensions::ExtensionRegistrar extension_registrar_;

  using InstallGateRegistry = std::map<extensions::ExtensionPrefs::DelayReason,
                                       extensions::InstallGate*>;
  InstallGateRegistry install_delayer_registry_;

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
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           RemoveExtensionFromBlacklist);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, BlacklistedInPrefsFromStartup);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           GreylistedExtensionDisabled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           GreylistDontEnableManuallyDisabled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           GreylistUnknownDontChange);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           ManagementPolicyProhibitsEnableOnInstalled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           BlockAndUnblockBlacklistedExtension);
  FRIEND_TEST_ALL_PREFIXES(BlacklistedExtensionSyncServiceTest,
                           SyncBlacklistedExtension);
  friend class BlacklistedExtensionSyncServiceTest;

  DISALLOW_COPY_AND_ASSIGN(ExtensionService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
