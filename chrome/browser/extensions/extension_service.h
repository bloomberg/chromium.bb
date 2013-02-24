// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/string16.h"
#include "chrome/browser/extensions/app_shortcut_manager.h"
#include "chrome/browser/extensions/app_sync_bundle.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_function_histogram_value.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_sync_bundle.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/external_provider_interface.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "sync/api/syncable_service.h"

class CommandLine;
class ExtensionErrorUI;
class ExtensionSyncData;
class ExtensionToolbarModel;
class GURL;
class Profile;
class Version;

namespace base {
class SequencedTaskRunner;
}

namespace extensions {
class AppNotificationManager;
class AppSyncData;
class BrowserEventRouter;
class ComponentLoader;
class ContentSettingsStore;
class CrxInstaller;
class ExtensionActionStorageManager;
class ExtensionSyncData;
class ExtensionSystem;
class ExtensionUpdater;
class PendingExtensionManager;
class SettingsFrontend;
}

namespace syncer {
class SyncErrorFactory;
}

// This is an interface class to encapsulate the dependencies that
// various classes have on ExtensionService. This allows easy mocking.
class ExtensionServiceInterface : public syncer::SyncableService {
 public:
  // A function that returns true if the given extension should be
  // included and false if it should be filtered out.  Identical to
  // PendingExtensionInfo::ShouldAllowInstallPredicate.
  typedef bool (*ExtensionFilter)(const extensions::Extension&);

  virtual ~ExtensionServiceInterface() {}
  virtual const ExtensionSet* extensions() const = 0;
  virtual const ExtensionSet* disabled_extensions() const = 0;
  virtual extensions::PendingExtensionManager* pending_extension_manager() = 0;

  // Install an update.  Return true if the install can be started.
  // Set out_crx_installer to the installer if one was started.
  virtual bool UpdateExtension(
      const std::string& id,
      const base::FilePath& path,
      const GURL& download_url,
      extensions::CrxInstaller** out_crx_installer) = 0;
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id,
      bool include_disabled) const = 0;
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const = 0;

  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const = 0;
  virtual void FinishDelayedInstallation(const std::string& extension_id) = 0;

  virtual bool IsExtensionEnabled(const std::string& extension_id) const = 0;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const = 0;

  virtual void CheckManagementPolicy() = 0;

  // Safe to call multiple times in a row.
  //
  // TODO(akalin): Remove this method (and others) once we refactor
  // themes sync to not use it directly.
  virtual void CheckForUpdatesSoon() = 0;

  virtual void AddExtension(const extensions::Extension* extension) = 0;
  virtual void AddComponentExtension(
      const extensions::Extension* extension) = 0;

  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) = 0;

  virtual void SyncExtensionChangeIfNeeded(
      const extensions::Extension& extension) = 0;

  virtual bool is_ready() = 0;

  // Returns task runner for crx installation file I/O operations.
  virtual base::SequencedTaskRunner* GetFileTaskRunner() = 0;
};

// Manages installed and running Chromium extensions.
class ExtensionService
    : public ExtensionServiceInterface,
      public extensions::ExternalProviderInterface::VisitorInterface,
      public content::NotificationObserver,
      public extensions::Blacklist::Observer {
 public:
  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char kInstallDirectoryName[];

  // If auto-updates are turned on, default to running every 5 hours.
  static const int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

  // The name of the directory inside the profile where per-app local settings
  // are stored.
  static const char kLocalAppSettingsDirectoryName[];

  // The name of the directory inside the profile where per-extension local
  // settings are stored.
  static const char kLocalExtensionSettingsDirectoryName[];

  // The name of the directory inside the profile where per-app synced settings
  // are stored.
  static const char kSyncAppSettingsDirectoryName[];

  // The name of the directory inside the profile where per-extension synced
  // settings are stored.
  static const char kSyncExtensionSettingsDirectoryName[];

  // The name of the directory inside the profile where per-extension persistent
  // managed settings are stored.
  static const char kManagedSettingsDirectoryName[];

  // The name of the database inside the profile where chrome-internal
  // extension state resides.
  static const char kStateStoreName[];

  // The name of the database inside the profile where declarative extension
  // rules are stored.
  static const char kRulesStoreName[];

  // Returns the Extension of hosted or packaged apps, NULL otherwise.
  const extensions::Extension* GetInstalledApp(const GURL& url) const;

  // Returns whether the URL is from either a hosted or packaged app.
  bool IsInstalledApp(const GURL& url) const;

  // If the renderer is hosting an installed app with isolated storage,
  // returns it, otherwise returns NULL.
  const extensions::Extension* GetIsolatedAppForRenderer(
      int renderer_child_id) const;

  // Attempts to uninstall an extension from a given ExtensionService. Returns
  // true iff the target extension exists.
  static bool UninstallExtensionHelper(ExtensionService* extensions_service,
                                       const std::string& extension_id);

  // Constructor stores pointers to |profile| and |extension_prefs| but
  // ownership remains at caller.
  ExtensionService(Profile* profile,
                   const CommandLine* command_line,
                   const base::FilePath& install_directory,
                   extensions::ExtensionPrefs* extension_prefs,
                   extensions::Blacklist* blacklist,
                   bool autoupdate_enabled,
                   bool extensions_enabled);

  virtual ~ExtensionService();

  // Gets the list of currently installed extensions.
  virtual const ExtensionSet* extensions() const OVERRIDE;
  virtual const ExtensionSet* disabled_extensions() const OVERRIDE;
  const ExtensionSet* terminated_extensions() const;
  const ExtensionSet* blacklisted_extensions() const;

  // Returns a set of all installed, disabled, blacklisted, and terminated
  // extensions.
  scoped_ptr<const ExtensionSet> GenerateInstalledExtensionsSet() const;

  // Returns a set of all extensions disabled by the sideload wipeout
  // initiative.
  scoped_ptr<const ExtensionSet> GetWipedOutExtensions() const;

  // Gets the object managing the set of pending extensions.
  virtual extensions::PendingExtensionManager*
      pending_extension_manager() OVERRIDE;

  const base::FilePath& install_directory() const { return install_directory_; }

  extensions::ProcessMap* process_map() { return &process_map_; }

  // Whether this extension can run in an incognito window.
  virtual bool IsIncognitoEnabled(const std::string& extension_id) const;
  virtual void SetIsIncognitoEnabled(const std::string& extension_id,
                                     bool enabled);

  // When app notification setup is done, we call this to save the developer's
  // oauth client id which we'll need at uninstall time to revoke the oauth
  // permission grant for sending notifications.
  virtual void SetAppNotificationSetupDone(const std::string& extension_id,
                                           const std::string& oauth_client_id);

  virtual void SetAppNotificationDisabled(const std::string& extension_id,
      bool value);

  // Updates the app launcher value for the moved extension so that it is now
  // located after the given predecessor and before the successor. This will
  // trigger a sync if needed. Empty strings are used to indicate no successor
  // or predecessor.
  void OnExtensionMoved(const std::string& moved_extension_id,
                        const std::string& predecessor_extension_id,
                        const std::string& successor_extension_id);

  // Returns true if the given extension can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  bool CanCrossIncognito(const extensions::Extension* extension) const;

  // Returns true if the given extension can be loaded in incognito.
  bool CanLoadInIncognito(const extensions::Extension* extension) const;

  // Whether this extension can inject scripts into pages with file URLs.
  bool AllowFileAccess(const extensions::Extension* extension) const;
  // Will reload the extension since this permission is applied at loading time
  // only.
  void SetAllowFileAccess(const extensions::Extension* extension, bool allow);

  // Whether the persistent background page, if any, is ready. We don't load
  // other components until then. If there is no background page, or if it is
  // non-persistent (lazy), we consider it to be ready.
  bool IsBackgroundPageReady(const extensions::Extension* extension) const;
  void SetBackgroundPageReady(const extensions::Extension* extension);

  // Getter and setter for the flag that specifies whether the extension is
  // being upgraded.
  bool IsBeingUpgraded(const extensions::Extension* extension) const;
  void SetBeingUpgraded(const extensions::Extension* extension, bool value);

  // Getter and setter for the flag that specifies if the extension has used
  // the webrequest API.
  // TODO(mpcomplete): remove. http://crbug.com/100411
  bool HasUsedWebRequest(const extensions::Extension* extension) const;
  void SetHasUsedWebRequest(const extensions::Extension* extension, bool value);

  // Initialize and start all installed extensions.
  void Init();

  // To delay some initialization until after import has finished, register
  // for the notification.
  // TODO(yoz): remove InitEventRoutersAterImport.
  void InitEventRoutersAfterImport();
  void RegisterForImportFinished();

  // Complete some initialization after being notified that import has finished.
  void InitAfterImport();

  // Start up the extension event routers.
  void InitEventRouters();

  // Called when the associated Profile is going to be destroyed.
  void Shutdown();

  // Look up an extension by ID. Does not include terminated
  // extensions.
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;

  enum IncludeFlag {
    INCLUDE_NONE        = 0,
    INCLUDE_ENABLED     = 1 << 0,
    INCLUDE_DISABLED    = 1 << 1,
    INCLUDE_TERMINATED  = 1 << 2,
    INCLUDE_BLACKLISTED = 1 << 3,
    INCLUDE_EVERYTHING = (1 << 4) - 1,
  };

  // Look up an extension by ID, selecting which sets to look in:
  //  * extensions()             --> INCLUDE_ENABLED
  //  * disabled_extensions()    --> INCLUDE_DISABLED
  //  * terminated_extensions()  --> INCLUDE_TERMINATED
  //  * blacklisted_extensions() --> INCLUDE_BLACKLISTED
  const extensions::Extension* GetExtensionById(const std::string& id,
                                                int include_mask) const;

  // Returns the site of the given |extension_id|. Suitable for use with
  // BrowserContext::GetStoragePartitionForSite().
  GURL GetSiteForExtensionId(const std::string& extension_id);

  // Looks up a terminated (crashed) extension by ID.
  const extensions::Extension*
      GetTerminatedExtension(const std::string& id) const;

  // Looks up an extension by ID, regardless of whether it's enabled,
  // disabled, blacklisted, or terminated.
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const OVERRIDE;

  // Updates a currently-installed extension with the contents from
  // |extension_path|.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual bool UpdateExtension(
      const std::string& id,
      const base::FilePath& extension_path,
      const GURL& download_url,
      extensions::CrxInstaller** out_crx_installer) OVERRIDE;

  // Reloads the specified extension, sending the onLaunched() event to it if it
  // currently has any window showing.
  void ReloadExtension(const std::string& extension_id);

  // Reloads an extension and sends it the onRestarted() event.
  void RestartExtension(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |external_uninstall| is a magical parameter
  // that is only used to send information to ExtensionPrefs, which external
  // callers should never set to true.
  //
  // We pass the |extension_id| by value to avoid having it deleted from under
  // us incase someone calls it with Extension::id() or another string that we
  // are going to delete in this function.
  //
  // TODO(aa): Remove |external_uninstall| -- this information should be passed
  // to ExtensionPrefs some other way.
  virtual bool UninstallExtension(std::string extension_id,
                                  bool external_uninstall,
                                  string16* error);

  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const OVERRIDE;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const OVERRIDE;

  // Whether the extension should show as enabled state in launcher.
  bool IsExtensionEnabledForLauncher(const std::string& extension_id) const;

  // Enables the extension.  If the extension is already enabled, does
  // nothing.
  virtual void EnableExtension(const std::string& extension_id);

  // Disables the extension.  If the extension is already disabled, or
  // cannot be disabled, does nothing.
  virtual void DisableExtension(const std::string& extension_id,
      extensions::Extension::DisableReason disable_reason);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extension|'s manifest and re-enables the
  // extension.
  void GrantPermissionsAndEnableExtension(
      const extensions::Extension* extension,
      bool record_oauth2_grant);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extensions|'s manifest.
  void GrantPermissions(
      const extensions::Extension* extension,
      bool record_oauth2_grant);

  // Check for updates (or potentially new extensions from external providers)
  void CheckForExternalUpdates();

  // Unload the specified extension.
  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) OVERRIDE;

  // Unload all extensions. This is currently only called on shutdown, and
  // does not send notifications.
  void UnloadAllExtensions();

  // Called only by testing.
  void ReloadExtensions();

  // Scan the extension directory and clean up the cruft.
  void GarbageCollectExtensions();

  // Notifies Sync (if needed) of a newly-installed extension or a change to
  // an existing extension.
  virtual void SyncExtensionChangeIfNeeded(
      const extensions::Extension& extension) OVERRIDE;

  // Returns true if |url| should get extension api bindings and be permitted
  // to make api calls. Note that this is independent of what extension
  // permissions the given extension has been granted.
  bool ExtensionBindingsAllowed(const GURL& url);

  // Returns true if a normal browser window should avoid showing |url| in a
  // tab. In this case, |url| is also rewritten to an error URL.
  bool ShouldBlockUrlInBrowserTab(GURL* url);

  // Called when the initial extensions load has completed.
  virtual void OnLoadedInstalledExtensions();

  // Adds |extension| to this ExtensionService and notifies observers that the
  // extensions have been loaded.
  virtual void AddExtension(const extensions::Extension* extension) OVERRIDE;

  // Check if we have preferences for the component extension and, if not or if
  // the stored version differs, install the extension (without requirements
  // checking) before calling AddExtension.
  virtual void AddComponentExtension(const extensions::Extension* extension)
      OVERRIDE;

  // Launch an extension the next time it is loaded.
  void ScheduleLaunchOnLoad(const std::string& extension_id);

  // Informs the service that an extension's files are in place for loading.
  //
  // Please make sure the Blacklist is checked some time before calling this
  // method.
  void OnExtensionInstalled(
      const extensions::Extension* extension,
      const syncer::StringOrdinal& page_ordinal,
      bool has_requirement_errors,
      bool wait_for_idle);

  // Similar to FinishInstallation, but first checks if there still is an update
  // pending for the extension, and makes sure the extension is still idle.
  void MaybeFinishDelayedInstallation(const std::string& extension_id);

  // Finishes installation of an update for an extension with the specified id,
  // when installation of that extension was previously delayed because the
  // extension was in use.
  virtual void FinishDelayedInstallation(
     const std::string& extension_id) OVERRIDE;

  // Returns an update for an extension with the specified id, if installation
  // of that update was previously delayed because the extension was in use. If
  // no updates are pending for the extension returns NULL.
  virtual const extensions::Extension* GetPendingExtensionUpdate(
      const std::string& extension_id) const OVERRIDE;

  // Initializes the |extension|'s active permission set and disables the
  // extension if the privilege level has increased (e.g., due to an upgrade).
  void InitializePermissions(const extensions::Extension* extension);

  // Check to see if this extension needs to be disabled, as per the sideload
  // wipeout initiative.
  void MaybeWipeout(const extensions::Extension* extension);

  // Go through each extension and unload those that are not allowed to run by
  // management policy providers (ie. network admin and Google-managed
  // blacklist).
  virtual void CheckManagementPolicy() OVERRIDE;

  virtual void CheckForUpdatesSoon() OVERRIDE;

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Gets the sync data for the given extension, assuming that the extension is
  // syncable.
  extensions::ExtensionSyncData GetExtensionSyncData(
      const extensions::Extension& extension) const;

  // Gets the sync data for the given app, assuming that the app is
  // syncable.
  extensions::AppSyncData GetAppSyncData(
      const extensions::Extension& extension) const;

  // Gets the ExtensionSyncData for all extensions.
  std::vector<extensions::ExtensionSyncData> GetExtensionSyncDataList() const;

  // Gets the AppSyncData for all extensions.
  std::vector<extensions::AppSyncData> GetAppSyncDataList() const;

  // Applies the change specified passed in by either ExtensionSyncData or
  // AppSyncData to the current system.
  // Returns false if the changes were not completely applied and were added
  // to the pending list to be tried again.
  bool ProcessExtensionSyncData(
      const extensions::ExtensionSyncData& extension_sync_data);
  bool ProcessAppSyncData(const extensions::AppSyncData& app_sync_data);


  void set_extensions_enabled(bool enabled) { extensions_enabled_ = enabled; }
  bool extensions_enabled() { return extensions_enabled_; }

  void set_show_extensions_prompts(bool enabled) {
    show_extensions_prompts_ = enabled;
  }

  bool show_extensions_prompts() {
    return show_extensions_prompts_;
  }

  Profile* profile();

  // TODO(skerner): Change to const ExtensionPrefs& extension_prefs() const,
  // ExtensionPrefs* mutable_extension_prefs().
  extensions::ExtensionPrefs* extension_prefs();

  extensions::SettingsFrontend* settings_frontend();

  extensions::ContentSettingsStore* GetContentSettingsStore();

  // Whether the extension service is ready.
  virtual bool is_ready() OVERRIDE;

  virtual base::SequencedTaskRunner* GetFileTaskRunner() OVERRIDE;

  extensions::ComponentLoader* component_loader() {
    return component_loader_.get();
  }

  // Note that this may return NULL if autoupdate is not turned on.
  extensions::ExtensionUpdater* updater();

  ExtensionToolbarModel* toolbar_model() { return &toolbar_model_; }

  ExtensionsQuotaService* quota_service() { return &quota_service_; }

  extensions::MenuManager* menu_manager() { return &menu_manager_; }

  extensions::AppNotificationManager* app_notification_manager() {
    return app_notification_manager_.get();
  }

  extensions::BrowserEventRouter* browser_event_router() {
    return browser_event_router_.get();
  }

  // Notify the frontend that there was an error loading an extension.
  // This method is public because UnpackedInstaller and InstalledLoader
  // can post to here.
  // TODO(aa): Remove this. It doesn't do enough to be worth the dependency
  // of these classes on ExtensionService.
  void ReportExtensionLoadError(const base::FilePath& extension_path,
                                const std::string& error,
                                bool be_noisy);

  // ExtensionHost of background page calls this method right after its render
  // view has been created.
  void DidCreateRenderViewForBackgroundPage(extensions::ExtensionHost* host);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, uninstall it.
  void CheckExternalUninstall(const std::string& id);

  // Changes sequenced task runner for crx installation tasks to |task_runner|.
  void SetFileTaskRunnerForTesting(base::SequencedTaskRunner* task_runner);

  // Clear all ExternalProviders.
  void ClearProvidersForTesting();

  // Adds an ExternalProviderInterface for the service to use during testing.
  // Takes ownership of |test_provider|.
  void AddProviderForTesting(
      extensions::ExternalProviderInterface* test_provider);

  // ExternalProvider::Visitor implementation.
  virtual bool OnExternalExtensionFileFound(
      const std::string& id,
      const Version* version,
      const base::FilePath& path,
      extensions::Manifest::Location location,
      int creation_flags,
      bool mark_acknowledged) OVERRIDE;

  virtual bool OnExternalExtensionUpdateUrlFound(
      const std::string& id,
      const GURL& update_url,
      extensions::Manifest::Location location) OVERRIDE;

  virtual void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) OVERRIDE;

  // Returns true when all the external extension providers are ready.
  bool AreAllExternalProvidersReady() const;

  void OnAllExternalProvidersReady();

  // Once all external providers are done, generates any needed alerts about
  // extensions.
  void IdentifyAlertableExtensions();

  // Given an ExtensionErrorUI alert, populates it with any extensions that
  // need alerting. Returns true if the alert should be displayed at all.
  //
  // This method takes the extension_error_ui argument rather than using
  // the member variable to make it easier to test the method in isolation.
  bool PopulateExtensionErrorUI(ExtensionErrorUI* extension_error_ui);

  // Checks if there are any new external extensions to notify the user about.
  void UpdateExternalExtensionAlert();

  // Given a (presumably just-installed) extension id, mark that extension as
  // acknowledged.
  void AcknowledgeExternalExtension(const std::string& id);

  // Returns true if this extension is an external one that has yet to be
  // marked as acknowledged.
  bool IsUnacknowledgedExternalExtension(
      const extensions::Extension* extension);

  // Opens the Extensions page because the user wants to get more details
  // about the alerts.
  void HandleExtensionAlertDetails();

  // Called when the extension alert is closed.
  void HandleExtensionAlertClosed();

  // Marks alertable extensions as acknowledged, after the user presses the
  // accept button.
  void HandleExtensionAlertAccept();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Whether there are any apps installed. Component apps are not included.
  bool HasApps() const;

  // Gets the set of loaded app ids. Component apps are not included.
  extensions::ExtensionIdSet GetAppIds() const;

  // Record a histogram using the PermissionMessage enum values for each
  // permission in |e|.
  // NOTE: If this is ever called with high frequency, the implementation may
  // need to be made more efficient.
  static void RecordPermissionMessagesHistogram(
      const extensions::Extension* e, const char* histogram);

  // Open a dev tools window for the background page for the given extension,
  // starting the background page first if necessary.
  void InspectBackgroundPage(const extensions::Extension* extension);

#if defined(UNIT_TEST)
  void TrackTerminatedExtensionForTest(const extensions::Extension* extension) {
    TrackTerminatedExtension(extension);
  }

  void FinishInstallationForTest(const extensions::Extension* extension) {
    FinishInstallation(extension);
  }
#endif

  extensions::AppShortcutManager* app_shortcut_manager() {
    return &app_shortcut_manager_;
  }

  // Specialization of syncer::SyncableService::AsWeakPtr.
  base::WeakPtr<ExtensionService> AsWeakPtr() { return base::AsWeakPtr(this); }

  bool browser_terminating() const { return browser_terminating_; }

  // For testing.
  void set_browser_terminating_for_test(bool value) {
    browser_terminating_ = value;
  }

  // By default ExtensionService will wait with installing an updated extension
  // until the extension is idle. Tests might not like this behavior, so you can
  // disable it with this method.
  void set_install_updates_when_idle_for_test(bool value) {
    install_updates_when_idle_ = value;
  }

 private:
  // Contains Extension data that can change during the life of the process,
  // but does not persist across restarts.
  struct ExtensionRuntimeData {
    // True if the background page is ready.
    bool background_page_ready;

    // True while the extension is being upgraded.
    bool being_upgraded;

    // True if the extension has used the webRequest API.
    bool has_used_webrequest;

    ExtensionRuntimeData();
    ~ExtensionRuntimeData();
  };
  typedef std::map<std::string, ExtensionRuntimeData> ExtensionRuntimeDataMap;

  struct NaClModuleInfo {
    NaClModuleInfo();
    ~NaClModuleInfo();

    GURL url;
    std::string mime_type;
  };
  typedef std::list<NaClModuleInfo> NaClModuleInfoList;

  // Return true if the sync type of |extension| matches |type|.
  bool IsCorrectSyncType(const extensions::Extension& extension,
                         syncer::ModelType type)
      const;

  void OnExtensionInstallPrefChanged();

  // Handles setting the extension specific values in |extension_sync_data| to
  // the current system.
  // Returns false if the changes were not completely applied and need to be
  // tried again later.
  bool ProcessExtensionSyncDataHelper(
      const extensions::ExtensionSyncData& extension_sync_data,
      syncer::ModelType type);

  // Events to be fired after an extension is reloaded.
  enum PostReloadEvents {
    EVENT_NONE = 0,
    EVENT_LAUNCHED = 1 << 0,
    EVENT_RESTARTED = 1 << 1,
  };

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
  void AddNewOrUpdatedExtension(const extensions::Extension* extension,
                                extensions::Extension::State initial_state,
                                const syncer::StringOrdinal& page_ordinal);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const extensions::Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(const extensions::Extension* extension,
                               extension_misc::UnloadedExtensionReason reason);

  // Common helper to finish installing the given extension.
  void FinishInstallation(const extensions::Extension* extension);

  // Reloads |extension_id| and then dispatches to it the PostReloadEvents
  // indicated by |events|.
  void ReloadExtensionWithEvents(const std::string& extension_id,
                                int events);

  // Returns true if the app with id |extension_id| has any shell windows open.
  bool HasShellWindows(const std::string& extension_id);

  // Helper that updates the active extension list used for crash reporting.
  void UpdateActiveExtensionsInCrashReporter();

  // We implement some Pepper plug-ins using NaCl to take advantage of NaCl's
  // strong sandbox. Typically, these NaCl modules are stored in extensions
  // and registered here. Not all NaCl modules need to register for a MIME
  // type, just the ones that are responsible for rendering a particular MIME
  // type, like application/pdf. Note: We only register NaCl modules in the
  // browser process.
  void RegisterNaClModule(const GURL& url, const std::string& mime_type);
  void UnregisterNaClModule(const GURL& url);

  // Call UpdatePluginListWithNaClModules() after registering or unregistering
  // a NaCl module to see those changes reflected in the PluginList.
  void UpdatePluginListWithNaClModules();

  NaClModuleInfoList::iterator FindNaClModule(const GURL& url);

  // Performs tasks requested to occur after |extension| loads.
  void DoPostLoadTasks(const extensions::Extension* extension);

  // Launches the platform app associated with |extension_host|.
  static void LaunchApplication(extensions::ExtensionHost* extension_host);

  // Dispatches a restart event to the platform app associated with
  // |extension_host|.
  static void RestartApplication(extensions::ExtensionHost* extension_host);

  // Helper to inspect an ExtensionHost after it has been loaded.
  void InspectExtensionHost(extensions::ExtensionHost* host);

  // Helper to determine whether we should initially enable an installed
  // (or upgraded) extension.
  bool ShouldEnableOnInstall(const extensions::Extension* extension);

  // Helper to determine if an extension is idle, and it should be safe
  // to update the extension.
  bool IsExtensionIdle(const std::string& extension_id) const;

  // Helper to determine if updating an extensions should proceed immediately,
  // or if we should delay the update until further notice.
  bool ShouldDelayExtensionUpdate(const std::string& extension_id,
                                  bool wait_for_idle) const;

  // Helper to search storage directories for extensions with isolated storage
  // that have been orphaned by an uninstall.
  void GarbageCollectIsolatedStorage();
  void OnGarbageCollectIsolatedStorageFinished();
  void OnNeedsToGarbageCollectIsolatedStorage();

  // extensions::Blacklist::Observer implementation.
  virtual void OnBlacklistUpdated() OVERRIDE;

  // Manages the blacklisted extensions, intended as callback from
  // Blacklist::GetBlacklistedIDs.
  void ManageBlacklist(const std::set<std::string>& old_blacklisted_ids,
                       const std::set<std::string>& new_blacklisted_ids);

  // Controls if installs are delayed. See comment for |installs_delayed_|.
  void set_installs_delayed(bool value) { installs_delayed_ = value; }
  bool installs_delayed() const { return installs_delayed_; }

  // The normal profile associated with this ExtensionService.
  Profile* profile_;

  // The ExtensionSystem for the profile above.
  extensions::ExtensionSystem* system_;

  // Preferences for the owning profile.
  extensions::ExtensionPrefs* extension_prefs_;

  // Blacklist for the owning profile.
  extensions::Blacklist* blacklist_;

  // Settings for the owning profile.
  scoped_ptr<extensions::SettingsFrontend> settings_frontend_;

  // The current list of installed extensions.
  ExtensionSet extensions_;

  // The list of installed extensions that have been disabled.
  ExtensionSet disabled_extensions_;

  // The list of installed extensions that have been terminated.
  ExtensionSet terminated_extensions_;

  // The list of installed extensions that have been blacklisted. Generally
  // these shouldn't be considered as installed by the extension platform: we
  // only keep them around so that if extensions are blacklisted by mistake
  // they can easily be un-blacklisted.
  ExtensionSet blacklisted_extensions_;

  // The list of extension updates that have had their installs delayed because
  // they are waiting for idle.
  ExtensionSet delayed_updates_for_idle_;

  // The list of extension installs delayed by |installs_delayed_|.
  // This is a disjoint set from |delayed_updates_for_idle_|. Extensions in
  // the |delayed_installs_| do not need to wait for idle.
  ExtensionSet delayed_installs_;

  // Hold the set of pending extensions.
  extensions::PendingExtensionManager pending_extension_manager_;

  // The map of extension IDs to their runtime data.
  ExtensionRuntimeDataMap extension_runtime_data_;

  // The full path to the directory where extensions are installed.
  base::FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_;

  // Whether to notify users when they attempt to install an extension.
  bool show_extensions_prompts_;

  // Whether to delay installing of extension updates until the extension is
  // idle.
  bool install_updates_when_idle_;

  // Used by dispatchers to limit API quota for individual extensions.
  ExtensionsQuotaService quota_service_;

  // Record that Init() has been called, and chrome::EXTENSIONS_READY
  // has fired.
  bool ready_;

  // Our extension updater, if updates are turned on.
  scoped_ptr<extensions::ExtensionUpdater> updater_;

  // The model that tracks extensions with BrowserAction buttons.
  ExtensionToolbarModel toolbar_model_;

  // Map unloaded extensions' ids to their paths. When a temporarily loaded
  // extension is unloaded, we lose the information about it and don't have
  // any in the extension preferences file.
  typedef std::map<std::string, base::FilePath> UnloadedExtensionPathMap;
  UnloadedExtensionPathMap unloaded_extension_paths_;

  // Map disabled extensions' ids to their paths. When a temporarily loaded
  // extension is disabled before it is reloaded, keep track of the path so that
  // it can be re-enabled upon a successful load.
  typedef std::map<std::string, base::FilePath> DisabledExtensionPathMap;
  DisabledExtensionPathMap disabled_extension_paths_;

  // Map of inspector cookies that are detached, waiting for an extension to be
  // reloaded.
  typedef std::map<std::string, int> OrphanedDevTools;
  OrphanedDevTools orphaned_dev_tools_;

  // Maps extension ids to a bitmask that indicates which events should be
  // dispatched to the extension when it is loaded.
  std::map<std::string, int> on_load_events_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Keeps track of loading and unloading component extensions.
  scoped_ptr<extensions::ComponentLoader> component_loader_;

  // Keeps track of menu items added by extensions.
  extensions::MenuManager menu_manager_;

  // Keeps track of app notifications.
  scoped_refptr<extensions::AppNotificationManager> app_notification_manager_;

  // Flag to make sure event routers are only initialized once.
  bool event_routers_initialized_;

  // TODO(yoz): None of these should be owned by ExtensionService.
  // crbug.com/159265
  scoped_ptr<extensions::BrowserEventRouter> browser_event_router_;

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

  // Set when the browser is terminating. Prevents us from installing or
  // updating additional extensions and allows in-progress installations to
  // decide to abort.
  bool browser_terminating_;

  // Set to true to delay all new extension installations. Acts as a lock
  // to allow background processing of tasks such as garbage collection of
  // on-disk state without needing to worry about race conditions caused
  // by extension installation and reinstallation.
  bool installs_delayed_;

  // Whether any extension should be considered for wipeout.
  bool wipeout_is_active_;

  // How many extensions were wiped out.
  size_t wipeout_count_;

  NaClModuleInfoList nacl_module_list_;

  extensions::AppSyncBundle app_sync_bundle_;
  extensions::ExtensionSyncBundle extension_sync_bundle_;

  extensions::ProcessMap process_map_;

  extensions::AppShortcutManager app_shortcut_manager_;

  scoped_ptr<ExtensionErrorUI> extension_error_ui_;
  // Sequenced task runner for extension related file operations.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<extensions::ExtensionActionStorageManager>
      extension_action_storage_manager_;
#endif

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           InstallAppsWithUnlimtedStorage);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           InstallAppsAndCheckStorageProtection);
  DISALLOW_COPY_AND_ASSIGN(ExtensionService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
