// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/property_bag.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/browser/extensions/extension_permissions_api.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/external_extension_provider_interface.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AppNotificationManager;
class BookmarkExtensionEventRouter;
class CrxInstaller;
class ExtensionBrowserEventRouter;
class ExtensionContentSettingsStore;
class ExtensionCookiesEventRouter;
class ExtensionDownloadsEventRouter;
class ExtensionFileBrowserEventRouter;
class ExtensionGlobalError;
class ExtensionManagementEventRouter;
class ExtensionPreferenceEventRouter;
class ExtensionSyncData;
class ExtensionToolbarModel;
class ExtensionUpdater;
class ExtensionWebNavigationEventRouter;
class HistoryExtensionEventRouter;
class GURL;
class PendingExtensionManager;
class Profile;
class SyncData;
class Version;

namespace chromeos {
class ExtensionInputMethodEventRouter;
}

namespace extensions {
class ComponentLoader;
class SettingsFrontend;
}

// This is an interface class to encapsulate the dependencies that
// various classes have on ExtensionService. This allows easy mocking.
class ExtensionServiceInterface : public SyncableService {
 public:
  // A function that returns true if the given extension should be
  // included and false if it should be filtered out.  Identical to
  // PendingExtensionInfo::ShouldAllowInstallPredicate.
  typedef bool (*ExtensionFilter)(const Extension&);

  virtual ~ExtensionServiceInterface() {}
  virtual const ExtensionList* extensions() const = 0;
  virtual PendingExtensionManager* pending_extension_manager() = 0;

  // Install an update.  Return true if the install can be started.
  // Set out_crx_installer to the installer if one was started.
  virtual bool UpdateExtension(
      const std::string& id,
      const FilePath& path,
      const GURL& download_url,
      CrxInstaller** out_crx_installer) = 0;
  virtual const Extension* GetExtensionById(const std::string& id,
                                            bool include_disabled) const = 0;
  virtual const Extension* GetInstalledExtension(
      const std::string& id) const = 0;

  virtual bool IsExtensionEnabled(const std::string& extension_id) const = 0;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const = 0;

  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) = 0;
  virtual void CheckAdminBlacklist() = 0;

  // Safe to call multiple times in a row.
  //
  // TODO(akalin): Remove this method (and others) once we refactor
  // themes sync to not use it directly.
  virtual void CheckForUpdatesSoon() = 0;

  virtual void AddExtension(const Extension* extension) = 0;

  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) = 0;

  virtual bool is_ready() = 0;
};

// Manages installed and running Chromium extensions.
class ExtensionService
    : public ExtensionServiceInterface,
      public ExternalExtensionProviderInterface::VisitorInterface,
      public base::SupportsWeakPtr<ExtensionService>,
      public content::NotificationObserver {
 public:
  using base::SupportsWeakPtr<ExtensionService>::AsWeakPtr;

  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

  // If auto-updates are turned on, default to running every 5 hours.
  static const int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

  // The name of the directory inside the profile where per-extension settings
  // are stored.
  static const char* kExtensionSettingsDirectoryName;

  // The name of the directory inside the profile where per-app settings
  // are stored.
  static const char* kAppSettingsDirectoryName;

  // Determine if a given extension download should be treated as if it came
  // from the gallery. Note that this is requires *both* that the download_url
  // match and that the download was referred from a gallery page.
  bool IsDownloadFromGallery(const GURL& download_url,
                             const GURL& referrer_url);

  // Returns the Extension of hosted or packaged apps, NULL otherwise.
  const Extension* GetInstalledApp(const GURL& url);

  // Returns whether the URL is from either a hosted or packaged app.
  bool IsInstalledApp(const GURL& url);

  // Associates a renderer process with the given installed app.
  void SetInstalledAppForRenderer(int renderer_child_id, const Extension* app);

  // If the renderer is hosting an installed app, returns it, otherwise returns
  // NULL.
  const Extension* GetInstalledAppForRenderer(int renderer_child_id);

  // Attempts to uninstall an extension from a given ExtensionService. Returns
  // true iff the target extension exists.
  static bool UninstallExtensionHelper(ExtensionService* extensions_service,
                                       const std::string& extension_id);

  // Constructor stores pointers to |profile| and |extension_prefs| but
  // ownership remains at caller.
  ExtensionService(Profile* profile,
                   const CommandLine* command_line,
                   const FilePath& install_directory,
                   ExtensionPrefs* extension_prefs,
                   bool autoupdate_enabled,
                   bool extensions_enabled);

  virtual ~ExtensionService();

  // Gets the list of currently installed extensions.
  virtual const ExtensionList* extensions() const OVERRIDE;
  const ExtensionList* disabled_extensions() const;
  const ExtensionList* terminated_extensions() const;

  // Gets the object managing the set of pending extensions.
  virtual PendingExtensionManager* pending_extension_manager() OVERRIDE;

  const FilePath& install_directory() const { return install_directory_; }

  AppsPromo* apps_promo() { return &apps_promo_; }

  extensions::ProcessMap* process_map() { return &process_map_; }

  // Whether this extension can run in an incognito window.
  virtual bool IsIncognitoEnabled(const std::string& extension_id) const;
  virtual void SetIsIncognitoEnabled(const std::string& extension_id,
                                     bool enabled);

  virtual void SetAppNotificationSetupDone(const std::string& extension_id,
      bool value);

  virtual void SetAppNotificationDisabled(const std::string& extension_id,
      bool value);

  // Returns true if the given extension can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  bool CanCrossIncognito(const Extension* extension);

  // Returns true if the given extension can be loaded in incognito.
  bool CanLoadInIncognito(const Extension* extension) const;

  // Whether this extension can inject scripts into pages with file URLs.
  bool AllowFileAccess(const Extension* extension);
  // Will reload the extension since this permission is applied at loading time
  // only.
  void SetAllowFileAccess(const Extension* extension, bool allow);

  // Getter and setter for the Browser Action visibility in the toolbar.
  bool GetBrowserActionVisibility(const Extension* extension);
  void SetBrowserActionVisibility(const Extension* extension, bool visible);

  // Whether the background page, if any, is ready. We don't load other
  // components until then. If there is no background page, we consider it to
  // be ready.
  bool IsBackgroundPageReady(const Extension* extension);
  void SetBackgroundPageReady(const Extension* extension);

  // Getter and setter for the flag that specifies whether the extension is
  // being upgraded.
  bool IsBeingUpgraded(const Extension* extension);
  void SetBeingUpgraded(const Extension* extension, bool value);

  // Getter and setter for the flag that specifies if the extension has used
  // the webrequest API.
  // TODO(mpcomplete): remove. http://crbug.com/100411
  bool HasUsedWebRequest(const Extension* extension);
  void SetHasUsedWebRequest(const Extension* extension, bool value);

  // Getter for the extension's runtime data PropertyBag.
  base::PropertyBag* GetPropertyBag(const Extension* extension);

  // Initialize and start all installed extensions.
  void Init();

  // Initialize the event routers after import has finished.
  void InitEventRoutersAfterImport();

  // Start up the extension event routers.
  void InitEventRouters();

  // Look up an extension by ID.  Does not include terminated
  // extensions.
  virtual const Extension* GetExtensionById(
      const std::string& id, bool include_disabled) const OVERRIDE;

  // Looks up a terminated (crashed) extension by ID.
  const Extension* GetTerminatedExtension(const std::string& id) const;

  // Looks up an extension by ID, regardless of whether it's enabled,
  // disabled, or terminated.
  virtual const Extension* GetInstalledExtension(
      const std::string& id) const OVERRIDE;

  // Updates a currently-installed extension with the contents from
  // |extension_path|.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual bool UpdateExtension(
      const std::string& id,
      const FilePath& extension_path,
      const GURL& download_url,
      CrxInstaller** out_crx_installer) OVERRIDE;

  // Reloads the specified extension.
  void ReloadExtension(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |external_uninstall| is a magical parameter
  // that is only used to send information to ExtensionPrefs, which external
  // callers should never set to true.
  // TODO(aa): Remove |external_uninstall| -- this information should be passed
  // to ExtensionPrefs some other way.
  virtual bool UninstallExtension(const std::string& extension_id,
                                  bool external_uninstall,
                                  std::string* error);

  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const OVERRIDE;
  virtual bool IsExternalExtensionUninstalled(
      const std::string& extension_id) const OVERRIDE;

  // Enables the extension.  If the extension is already enabled, does
  // nothing.
  virtual void EnableExtension(const std::string& extension_id);

  // Disables the extension.  If the extension is already disabled, or
  // cannot be disabled, does nothing.
  virtual void DisableExtension(const std::string& extension_id);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extension|'s manifest.
  void GrantPermissions(const Extension* extension);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extension|'s manifest and re-enables the
  // extension.
  void GrantPermissionsAndEnableExtension(const Extension* extension);

  // Sets the |extension|'s active permissions to |permissions|.
  void UpdateActivePermissions(const Extension* extension,
                               const ExtensionPermissionSet* permissions);

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

  // The App that represents the web store.
  const Extension* GetWebStoreApp();

  // Lookup an extension by |url|.
  const Extension* GetExtensionByURL(const GURL& url);

  // Returns the extension whose web extent contains |url|.
  const Extension* GetExtensionByWebExtent(const GURL& url);

  // Returns the disabled extension whose web extent contains |url|.
  const Extension* GetDisabledExtensionByWebExtent(const GURL& url);

  // Returns an extension that contains any URL that overlaps with the given
  // extent, if one exists.
  const Extension* GetExtensionByOverlappingWebExtent(
      const URLPatternSet& extent);

  // Returns true if |url| should get extension api bindings and be permitted
  // to make api calls. Note that this is independent of what extension
  // permissions the given extension has been granted.
  bool ExtensionBindingsAllowed(const GURL& url);

  // Returns the icon to display in the omnibox for the given extension.
  const SkBitmap& GetOmniboxIcon(const std::string& extension_id);

  // Returns the icon to display in the omnibox popup window for the given
  // extension.
  const SkBitmap& GetOmniboxPopupIcon(const std::string& extension_id);

  // Called when the initial extensions load has completed.
  virtual void OnLoadedInstalledExtensions();

  // Adds |extension| to this ExtensionService and notifies observers than an
  // extension has been loaded.  Called by the backend after an extension has
  // been loaded from a file and installed.
  virtual void AddExtension(const Extension* extension) OVERRIDE;

  // Called by the backend when an extension has been installed.
  void OnExtensionInstalled(
      const Extension* extension, bool from_webstore, int page_index);

  // Initializes the |extension|'s active permission set and disables the
  // extension if the privilege level has increased (e.g., due to an upgrade).
  void InitializePermissions(const Extension* extension);

  // Go through each extensions in pref, unload blacklisted extensions
  // and update the blacklist state in pref.
  virtual void UpdateExtensionBlacklist(
      const std::vector<std::string>& blacklist) OVERRIDE;

  // Go through each extension and unload those that the network admin has
  // put on the blacklist (not to be confused with the Google managed blacklist
  // set of extensions.
  virtual void CheckAdminBlacklist() OVERRIDE;

  virtual void CheckForUpdatesSoon() OVERRIDE;

  // SyncableService implementation.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

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
  ExtensionPrefs* extension_prefs();

  extensions::SettingsFrontend* settings_frontend();

  ExtensionContentSettingsStore* GetExtensionContentSettingsStore();

  // Whether the extension service is ready.
  virtual bool is_ready() OVERRIDE;

  extensions::ComponentLoader* component_loader() {
    return component_loader_.get();
  }

  // Note that this may return NULL if autoupdate is not turned on.
  ExtensionUpdater* updater();

  ExtensionToolbarModel* toolbar_model() { return &toolbar_model_; }

  ExtensionsQuotaService* quota_service() { return &quota_service_; }

  ExtensionMenuManager* menu_manager() { return &menu_manager_; }

  AppNotificationManager* app_notification_manager() {
    return app_notification_manager_.get();
  }

  ExtensionPermissionsManager* permissions_manager() {
    return &permissions_manager_;
  }

  ExtensionBrowserEventRouter* browser_event_router() {
    return browser_event_router_.get();
  }

#if defined(OS_CHROMEOS)
  ExtensionFileBrowserEventRouter* file_browser_event_router() {
    return file_browser_event_router_.get();
  }
  chromeos::ExtensionInputMethodEventRouter* input_method_event_router() {
    return input_method_event_router_.get();
  }
#endif

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
  void DidCreateRenderViewForBackgroundPage(ExtensionHost* host);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so, uninstall it.
  void CheckExternalUninstall(const std::string& id);

  // Clear all ExternalExtensionProviders.
  void ClearProvidersForTesting();

  // Adds an ExternalExtensionProviderInterface for the service to use during
  // testing. Takes ownership of |test_provider|.
  void AddProviderForTesting(ExternalExtensionProviderInterface* test_provider);

  // ExternalExtensionProvider::Visitor implementation.
  virtual void OnExternalExtensionFileFound(const std::string& id,
                                            const Version* version,
                                            const FilePath& path,
                                            Extension::Location location,
                                            int creation_flags)
      OVERRIDE;

  virtual void OnExternalExtensionUpdateUrlFound(const std::string& id,
                                                 const GURL& update_url,
                                                 Extension::Location location)
      OVERRIDE;

  virtual void OnExternalProviderReady(
      const ExternalExtensionProviderInterface* provider) OVERRIDE;

  void OnAllExternalProvidersReady();

  // Once all external providers are done, generates any needed alerts about
  // extensions.
  void IdentifyAlertableExtensions();

  // Marks alertable extensions as acknowledged, after the user presses the
  // accept button.
  void HandleExtensionAlertAccept(const ExtensionGlobalError& global_error,
                                  Browser* browser);

  // Opens the Extensions page because the user wants to get more details
  // about the alerts.
  void HandleExtensionAlertDetails(const ExtensionGlobalError& global_error,
                                   Browser* browser);

  // Displays the extension alert in the last-active browser window.
  void ShowExtensionAlert(ExtensionGlobalError* global_error);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Whether there are any apps installed. Component apps are not included.
  bool HasApps() const;

  // Gets the set of loaded app ids. Component apps are not included.
  ExtensionIdSet GetAppIds() const;

  // Record a histogram using the PermissionMessage enum values for each
  // permission in |e|.
  // NOTE: If this is ever called with high frequency, the implementation may
  // need to be made more efficient.
  static void RecordPermissionMessagesHistogram(
      const Extension* e, const char* histogram);

#if defined(UNIT_TEST)
  void TrackTerminatedExtensionForTest(const Extension* extension) {
    TrackTerminatedExtension(extension);
  }
#endif

  ExtensionWarningSet* extension_warnings() {
    return &extension_warnings_;
  }

 private:
  // Bundle of type (app or extension)-specific sync stuff.
  struct SyncBundle {
    SyncBundle();
    ~SyncBundle();

    bool HasExtensionId(const std::string& id) const;
    bool HasPendingExtensionId(const std::string& id) const;

    ExtensionFilter filter;
    std::set<std::string> synced_extensions;
    std::map<std::string, ExtensionSyncData> pending_sync_data;
    SyncChangeProcessor* sync_processor;
  };

  // Contains Extension data that can change during the life of the process,
  // but does not persist across restarts.
  struct ExtensionRuntimeData {
    // True if the background page is ready.
    bool background_page_ready;

    // True while the extension is being upgraded.
    bool being_upgraded;

    // True if the extension has used the webRequest API.
    bool has_used_webrequest;

    // Generic bag of runtime data that users can associate with extensions.
    base::PropertyBag property_bag;

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

  // Notifies Sync (if needed) of a newly-installed extension or a change to
  // an existing extension.
  void SyncExtensionChangeIfNeeded(const Extension& extension);

  // Get the appropriate SyncBundle, given some representation of Sync data.
  SyncBundle* GetSyncBundleForExtension(const Extension& extension);
  SyncBundle* GetSyncBundleForExtensionSyncData(
      const ExtensionSyncData& extension_sync_data);
  SyncBundle* GetSyncBundleForModelType(syncable::ModelType type);
  const SyncBundle* GetSyncBundleForModelTypeConst(syncable::ModelType type)
      const;

  // Gets the ExtensionSyncData for all extensions.
  std::vector<ExtensionSyncData> GetSyncDataList(
      const SyncBundle& bundle) const;

  // Gets the sync data for the given extension, assuming that the extension is
  // syncable.
  ExtensionSyncData GetSyncData(const Extension& extension) const;

  // Appends sync data objects for every extension in |extensions|
  // that passes |filter|.
  void GetSyncDataListHelper(
      const ExtensionList& extensions,
      const SyncBundle& bundle,
      std::vector<ExtensionSyncData>* sync_data_list) const;

  // Applies the change specified in an ExtensionSyncData to the current system.
  void ProcessExtensionSyncData(
      const ExtensionSyncData& extension_sync_data,
      SyncBundle& bundle);

  // Look up an extension by ID, optionally including either or both of enabled
  // and disabled extensions.
  const Extension* GetExtensionByIdInternal(const std::string& id,
                                            bool include_enabled,
                                            bool include_disabled,
                                            bool include_terminated) const;

  // Adds the given extension to the list of terminated extensions if
  // it is not already there and unloads it.
  void TrackTerminatedExtension(const Extension* extension);

  // Removes the extension with the given id from the list of
  // terminated extensions if it is there.
  void UntrackTerminatedExtension(const std::string& id);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(const Extension* extension,
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

  // The profile this ExtensionService is part of.
  Profile* profile_;

  // Preferences for the owning profile (weak reference).
  ExtensionPrefs* extension_prefs_;

  // Settings for the owning profile.
  scoped_ptr<extensions::SettingsFrontend> settings_frontend_;

  // The current list of installed extensions.
  // TODO(aa): This should use chrome/common/extensions/extension_set.h.
  ExtensionList extensions_;

  // The list of installed extensions that have been disabled.
  ExtensionList disabled_extensions_;

  // The list of installed extensions that have been terminated.
  ExtensionList terminated_extensions_;

  // Used to quickly check if an extension was terminated.
  std::set<std::string> terminated_extension_ids_;

  // Hold the set of pending extensions.
  PendingExtensionManager pending_extension_manager_;

  // The map of extension IDs to their runtime data.
  ExtensionRuntimeDataMap extension_runtime_data_;

  // Holds a map between renderer process IDs that are associated with an
  // installed app and their app.
  typedef std::map<int, scoped_refptr<const Extension> > InstalledAppMap;
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
  scoped_ptr<ExtensionUpdater> updater_;

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

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Keeps track of loading and unloading component extensions.
  scoped_ptr<extensions::ComponentLoader> component_loader_;

  // Keeps track of menu items added by extensions.
  ExtensionMenuManager menu_manager_;

  // Keeps track of app notifications.
  scoped_refptr<AppNotificationManager> app_notification_manager_;

  // Keeps track of extension permissions.
  ExtensionPermissionsManager permissions_manager_;

  // Keeps track of favicon-sized omnibox icons for extensions.
  ExtensionIconManager omnibox_icon_manager_;
  ExtensionIconManager omnibox_popup_icon_manager_;

  // Manages the promotion of the web store.
  AppsPromo apps_promo_;

  // Flag to make sure event routers are only initialized once.
  bool event_routers_initialized_;

  scoped_ptr<ExtensionDownloadsEventRouter> downloads_event_router_;

  scoped_ptr<HistoryExtensionEventRouter> history_event_router_;

  scoped_ptr<ExtensionBrowserEventRouter> browser_event_router_;

  scoped_ptr<ExtensionPreferenceEventRouter> preference_event_router_;

  scoped_ptr<BookmarkExtensionEventRouter> bookmark_event_router_;

  scoped_ptr<ExtensionCookiesEventRouter> cookies_event_router_;

  scoped_ptr<ExtensionManagementEventRouter> management_event_router_;

  scoped_ptr<ExtensionWebNavigationEventRouter> web_navigation_event_router_;

#if defined(OS_CHROMEOS)
  scoped_ptr<ExtensionFileBrowserEventRouter> file_browser_event_router_;
  scoped_ptr<chromeos::ExtensionInputMethodEventRouter>
      input_method_event_router_;
#endif

  // A collection of external extension providers.  Each provider reads
  // a source of external extension information.  Examples include the
  // windows registry and external_extensions.json.
  ProviderCollection external_extension_providers_;

  // Set to true by OnExternalExtensionUpdateUrlFound() when an external
  // extension URL is found.  Used in CheckForExternalUpdates() to see
  // if an update check is needed to install pending extensions.
  bool external_extension_url_added_;

  NaClModuleInfoList nacl_module_list_;

  SyncBundle app_sync_bundle_;
  SyncBundle extension_sync_bundle_;

  // Contains an entry for each warning that shall be currently shown.
  ExtensionWarningSet extension_warnings_;

  extensions::ProcessMap process_map_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           InstallAppsWithUnlimtedStorage);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           InstallAppsAndCheckStorageProtection);
  DISALLOW_COPY_AND_ASSIGN(ExtensionService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
