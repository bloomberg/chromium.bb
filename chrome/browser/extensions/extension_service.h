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
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/app_shortcut_manager.h"
#include "chrome/browser/extensions/app_sync_bundle.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_sync_bundle.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/external_provider_interface.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "sync/api/syncable_service.h"

class BookmarkExtensionEventRouter;
class CommandLine;
class ExtensionErrorUI;
class ExtensionFontSettingsEventRouter;
class ExtensionManagementEventRouter;
class ExtensionPreferenceEventRouter;
class ExtensionSyncData;
class ExtensionToolbarModel;
class HistoryExtensionEventRouter;
class GURL;
class Profile;
class Version;

namespace chromeos {
class ExtensionBluetoothEventRouter;
class ExtensionInputMethodEventRouter;
}

namespace extensions {
class AppNotificationManager;
class AppSyncData;
class BrowserEventRouter;
class ComponentLoader;
class ContentSettingsStore;
class CrxInstaller;
class Extension;
class ExtensionActionStorageManager;
class ExtensionCookiesEventRouter;
class ExtensionManagedModeEventRouter;
class ExtensionSyncData;
class ExtensionSystem;
class ExtensionUpdater;
class FontSettingsEventRouter;
class MediaGalleriesPrivateEventRouter;
class PendingExtensionManager;
class PushMessagingEventRouter;
class SettingsFrontend;
class WebNavigationEventRouter;
class WindowEventRouter;
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
      const FilePath& path,
      const GURL& download_url,
      extensions::CrxInstaller** out_crx_installer) = 0;
  virtual const extensions::Extension* GetExtensionById(const std::string& id,
                                            bool include_disabled) const = 0;
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const = 0;

  virtual bool IsExtensionEnabled(const std::string& extension_id) const = 0;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const = 0;

  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) = 0;
  virtual void CheckManagementPolicy() = 0;

  // Safe to call multiple times in a row.
  //
  // TODO(akalin): Remove this method (and others) once we refactor
  // themes sync to not use it directly.
  virtual void CheckForUpdatesSoon() = 0;

  virtual void AddExtension(const extensions::Extension* extension) = 0;

  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) = 0;

  virtual void SyncExtensionChangeIfNeeded(
      const extensions::Extension& extension) = 0;

  virtual bool is_ready() = 0;
};

// Manages installed and running Chromium extensions.
class ExtensionService
    : public ExtensionServiceInterface,
      public extensions::ExternalProviderInterface::VisitorInterface,
      public content::NotificationObserver {
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

  // The name of the database inside the profile where chrome-internal
  // extension state resides.
  static const char kStateStoreName[];

  // Returns the Extension of hosted or packaged apps, NULL otherwise.
  const extensions::Extension* GetInstalledApp(const GURL& url) const;

  // Returns whether the URL is from either a hosted or packaged app.
  bool IsInstalledApp(const GURL& url) const;

  // Associates a renderer process with the given installed app.
  void SetInstalledAppForRenderer(int renderer_child_id,
      const extensions::Extension* app);

  // If the renderer is hosting an installed app, returns it, otherwise returns
  // NULL.
  const extensions::Extension* GetInstalledAppForRenderer(
      int renderer_child_id) const;

  // Attempts to uninstall an extension from a given ExtensionService. Returns
  // true iff the target extension exists.
  static bool UninstallExtensionHelper(ExtensionService* extensions_service,
                                       const std::string& extension_id);

  // Constructor stores pointers to |profile| and |extension_prefs| but
  // ownership remains at caller.
  ExtensionService(Profile* profile,
                   const CommandLine* command_line,
                   const FilePath& install_directory,
                   extensions::ExtensionPrefs* extension_prefs,
                   bool autoupdate_enabled,
                   bool extensions_enabled);

  virtual ~ExtensionService();

  // Gets the list of currently installed extensions.
  virtual const ExtensionSet* extensions() const OVERRIDE;
  virtual const ExtensionSet* disabled_extensions() const OVERRIDE;
  const ExtensionSet* terminated_extensions() const;

  // Retuns a set of all installed, disabled, and terminated extensions and
  // transfers ownership to caller.
  const ExtensionSet* GenerateInstalledExtensionsSet() const;

  // Gets the object managing the set of pending extensions.
  virtual extensions::PendingExtensionManager*
      pending_extension_manager() OVERRIDE;

  const FilePath& install_directory() const { return install_directory_; }

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

  // Look up an extension by ID.  Does not include terminated
  // extensions.
  virtual const extensions::Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;

  // Looks up a terminated (crashed) extension by ID.
  const extensions::Extension*
      GetTerminatedExtension(const std::string& id) const;

  // Looks up an extension by ID, regardless of whether it's enabled,
  // disabled, or terminated.
  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& id) const OVERRIDE;

  // Updates a currently-installed extension with the contents from
  // |extension_path|.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual bool UpdateExtension(
      const std::string& id,
      const FilePath& extension_path,
      const GURL& download_url,
      extensions::CrxInstaller** out_crx_installer) OVERRIDE;

  // Reloads the specified extension.
  void ReloadExtension(const std::string& extension_id);

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

  // Returns the icon to display in the omnibox for the given extension.
  gfx::Image GetOmniboxIcon(const std::string& extension_id);

  // Returns the icon to display in the omnibox popup window for the given
  // extension.
  gfx::Image GetOmniboxPopupIcon(const std::string& extension_id);

  // Called when the initial extensions load has completed.
  virtual void OnLoadedInstalledExtensions();

  // Adds |extension| to this ExtensionService and notifies observers than an
  // extension has been loaded.  Called by the backend after an extension has
  // been loaded from a file and installed.
  virtual void AddExtension(const extensions::Extension* extension) OVERRIDE;

  // Called by the backend when an extension has been installed.
  void OnExtensionInstalled(
      const extensions::Extension* extension,
      bool from_webstore,
      const syncer::StringOrdinal& page_ordinal,
      bool has_requirement_errors);

  // Initializes the |extension|'s active permission set and disables the
  // extension if the privilege level has increased (e.g., due to an upgrade).
  void InitializePermissions(const extensions::Extension* extension);

  // Go through each extensions in pref, unload blacklisted extensions
  // and update the blacklist state in pref.
  virtual void UpdateExtensionBlacklist(
      const std::vector<std::string>& blacklist) OVERRIDE;

  // Go through each extension and unload those that are not allowed to run by
  // management policy providers (ie. network admin and Google-managed
  // blacklist).
  virtual void CheckManagementPolicy() OVERRIDE;

  virtual void CheckForUpdatesSoon() OVERRIDE;

  // syncer::SyncableService implementation.
  virtual syncer::SyncError MergeDataAndStartSyncing(
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

  extensions::WindowEventRouter* window_event_router() {
    return window_event_router_.get();
  }

#if defined(OS_CHROMEOS)
  chromeos::ExtensionBluetoothEventRouter* bluetooth_event_router() {
    return bluetooth_event_router_.get();
  }
  chromeos::ExtensionInputMethodEventRouter* input_method_event_router() {
    return input_method_event_router_.get();
  }
#endif

  extensions::PushMessagingEventRouter* push_messaging_event_router() {
    return push_messaging_event_router_.get();
  }

  // Notify the frontend that there was an error loading an extension.
  // This method is public because UnpackedInstaller and InstalledLoader
  // can post to here.
  // TODO(aa): Remove this. It doesn't do enough to be worth the dependency
  // of these classes on ExtensionService.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error,
                                bool be_noisy);

  // ExtensionHost of background page calls this method right after its render
  // view has been created.
  void DidCreateRenderViewForBackgroundPage(extensions::ExtensionHost* host);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, uninstall it.
  void CheckExternalUninstall(const std::string& id);

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
      const FilePath& path,
      extensions::Extension::Location location,
      int creation_flags,
      bool mark_acknowledged) OVERRIDE;

  virtual bool OnExternalExtensionUpdateUrlFound(
      const std::string& id,
      const GURL& update_url,
      extensions::Extension::Location location) OVERRIDE;

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

  // Marks alertable extensions as acknowledged, after the user presses the
  // accept button.
  void HandleExtensionAlertAccept();

  // Given a (presumably just-installed) extension id, mark that extension as
  // acknowledged.
  void AcknowledgeExternalExtension(const std::string& id);

  // Opens the Extensions page because the user wants to get more details
  // about the alerts.
  void HandleExtensionAlertDetails();

  // Called when the extension alert is closed.
  void HandleExtensionAlertClosed();

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

#if defined(UNIT_TEST)
  void TrackTerminatedExtensionForTest(const extensions::Extension* extension) {
    TrackTerminatedExtension(extension);
  }
#endif

  ExtensionWarningSet* extension_warnings() {
    return &extension_warnings_;
  }

  extensions::AppShortcutManager* app_shortcut_manager() {
    return &app_shortcut_manager_;
  }

  // Specialization of syncer::SyncableService::AsWeakPtr.
  base::WeakPtr<ExtensionService> AsWeakPtr() { return base::AsWeakPtr(this); }

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

  // Handles setting the extension specific values in |extension_sync_data| to
  // the current system.
  // Returns false if the changes were not completely applied and need to be
  // tried again later.
  bool ProcessExtensionSyncDataHelper(
      const extensions::ExtensionSyncData& extension_sync_data,
      syncer::ModelType type);

  enum IncludeFlag {
    INCLUDE_NONE = 0,
    INCLUDE_ENABLED = 1 << 0,
    INCLUDE_DISABLED = 1 << 1,
    INCLUDE_TERMINATED = 1 << 2
  };

  // Look up an extension by ID, optionally including either or both of enabled
  // and disabled extensions.
  const extensions::Extension* GetExtensionByIdInternal(
      const std::string& id,
      int include_mask) const;

  // Adds the given extension to the list of terminated extensions if
  // it is not already there and unloads it.
  void TrackTerminatedExtension(const extensions::Extension* extension);

  // Removes the extension with the given id from the list of
  // terminated extensions if it is there.
  void UntrackTerminatedExtension(const std::string& id);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const extensions::Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(const extensions::Extension* extension,
                               extension_misc::UnloadedExtensionReason reason);

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

  // Enqueues a launch task in the lazy background page queue.
  void QueueRestoreAppWindow(const extensions::Extension* extension);
  // Launches the platform app associated with |extension_host|.
  static void LaunchApplication(extensions::ExtensionHost* extension_host);

  // The normal profile associated with this ExtensionService.
  Profile* profile_;

  // The ExtensionSystem for the profile above.
  extensions::ExtensionSystem* system_;

  // Preferences for the owning profile (weak reference).
  extensions::ExtensionPrefs* extension_prefs_;

  // Settings for the owning profile.
  scoped_ptr<extensions::SettingsFrontend> settings_frontend_;

  // The current list of installed extensions.
  ExtensionSet extensions_;

  // The list of installed extensions that have been disabled.
  ExtensionSet disabled_extensions_;

  // The list of installed extensions that have been terminated.
  ExtensionSet terminated_extensions_;

  // Hold the set of pending extensions.
  extensions::PendingExtensionManager pending_extension_manager_;

  // The map of extension IDs to their runtime data.
  ExtensionRuntimeDataMap extension_runtime_data_;

  // Holds a map between renderer process IDs that are associated with an
  // installed app and their app.
  typedef std::map<int, scoped_refptr<const extensions::Extension> >
      InstalledAppMap;
  InstalledAppMap installed_app_hosts_;

  // The full path to the directory where extensions are installed.
  FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_;

  // Whether to notify users when they attempt to install an extension.
  bool show_extensions_prompts_;

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
  typedef std::map<std::string, FilePath> UnloadedExtensionPathMap;
  UnloadedExtensionPathMap unloaded_extension_paths_;

  // Map disabled extensions' ids to their paths. When a temporarily loaded
  // extension is disabled before it is reloaded, keep track of the path so that
  // it can be re-enabled upon a successful load.
  typedef std::map<std::string, FilePath> DisabledExtensionPathMap;
  DisabledExtensionPathMap disabled_extension_paths_;

  // Map of inspector cookies that are detached, waiting for an extension to be
  // reloaded.
  typedef std::map<std::string, int> OrphanedDevTools;
  OrphanedDevTools orphaned_dev_tools_;

  // A set of apps that had an open window the last time they were reloaded.
  // A new window will be launched when the app is succesfully reloaded.
  std::set<std::string> relaunch_app_ids_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Keeps track of loading and unloading component extensions.
  scoped_ptr<extensions::ComponentLoader> component_loader_;

  // Keeps track of menu items added by extensions.
  extensions::MenuManager menu_manager_;

  // Keeps track of app notifications.
  scoped_refptr<extensions::AppNotificationManager> app_notification_manager_;

  // Keeps track of favicon-sized omnibox icons for extensions.
  ExtensionIconManager omnibox_icon_manager_;
  ExtensionIconManager omnibox_popup_icon_manager_;

  // Flag to make sure event routers are only initialized once.
  bool event_routers_initialized_;

  scoped_ptr<HistoryExtensionEventRouter> history_event_router_;

  scoped_ptr<extensions::BrowserEventRouter> browser_event_router_;

  scoped_ptr<extensions::WindowEventRouter> window_event_router_;

  scoped_ptr<ExtensionPreferenceEventRouter> preference_event_router_;

  scoped_ptr<BookmarkExtensionEventRouter> bookmark_event_router_;

  scoped_ptr<extensions::ExtensionCookiesEventRouter> cookies_event_router_;

  scoped_ptr<ExtensionManagementEventRouter> management_event_router_;

  scoped_ptr<extensions::MediaGalleriesPrivateEventRouter>
      media_galleries_private_event_router_;

  scoped_ptr<extensions::PushMessagingEventRouter>
      push_messaging_event_router_;

  scoped_ptr<extensions::WebNavigationEventRouter> web_navigation_event_router_;

  scoped_ptr<extensions::FontSettingsEventRouter> font_settings_event_router_;

  scoped_ptr<extensions::ExtensionManagedModeEventRouter>
      managed_mode_event_router_;

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::ExtensionBluetoothEventRouter> bluetooth_event_router_;
  scoped_ptr<chromeos::ExtensionInputMethodEventRouter>
      input_method_event_router_;
#endif

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

  NaClModuleInfoList nacl_module_list_;

  extensions::AppSyncBundle app_sync_bundle_;
  extensions::ExtensionSyncBundle extension_sync_bundle_;

  // Contains an entry for each warning that shall be currently shown.
  ExtensionWarningSet extension_warnings_;

  extensions::ProcessMap process_map_;

  extensions::AppShortcutManager app_shortcut_manager_;

  scoped_ptr<ExtensionErrorUI> extension_error_ui_;

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
