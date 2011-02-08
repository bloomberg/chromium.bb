// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/extensions/external_extension_provider_interface.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/property_bag.h"

class ExtensionBrowserEventRouter;
class ExtensionServiceBackend;
class ExtensionToolbarModel;
class ExtensionUpdater;
class GURL;
class Profile;
class Version;

typedef bool (*ShouldInstallExtensionPredicate)(const Extension&);

// A pending extension is an extension that hasn't been installed yet
// and is intended to be installed in the next auto-update cycle.  The
// update URL of a pending extension may be blank, in which case a
// default one is assumed.
struct PendingExtensionInfo {
  PendingExtensionInfo(
      const GURL& update_url,
      ShouldInstallExtensionPredicate should_install_extension,
      bool is_from_sync,
      bool install_silently,
      bool enable_on_install,
      bool enable_incognito_on_install,
      Extension::Location install_source);

  PendingExtensionInfo();

  GURL update_url;
  // When the extension is about to be installed, this function is
  // called.  If this function returns true, the install proceeds.  If
  // this function returns false, the install is aborted.
  ShouldInstallExtensionPredicate should_install_extension;
  bool is_from_sync;  // This update check was initiated from sync.
  bool install_silently;
  bool enable_on_install;
  bool enable_incognito_on_install;
  Extension::Location install_source;
};

// A PendingExtensionMap is a map from IDs of pending extensions to
// their info.
typedef std::map<std::string, PendingExtensionInfo> PendingExtensionMap;

// This is an interface class to encapsulate the dependencies that
// ExtensionUpdater has on ExtensionService. This allows easy mocking.
class ExtensionUpdateService {
 public:
  virtual ~ExtensionUpdateService() {}
  virtual const ExtensionList* extensions() const = 0;
  virtual const PendingExtensionMap& pending_extensions() const = 0;
  virtual void UpdateExtension(const std::string& id, const FilePath& path,
                               const GURL& download_url) = 0;
  virtual const Extension* GetExtensionById(const std::string& id,
                                            bool include_disabled) = 0;
  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist) = 0;
  virtual void CheckAdminBlacklist() = 0;
  virtual bool HasInstalledExtensions() = 0;

  virtual ExtensionPrefs* extension_prefs() = 0;
  virtual Profile* profile() = 0;
};

// Manages installed and running Chromium extensions.
class ExtensionService
    : public base::RefCountedThreadSafe<ExtensionService,
                                        BrowserThread::DeleteOnUIThread>,
      public ExtensionUpdateService,
      public ExternalExtensionProviderInterface::VisitorInterface,
      public NotificationObserver {
 public:
  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(const std::string& manifest,
                           const FilePath& root_directory)
        : manifest(manifest),
          root_directory(root_directory) {
    }

    // The extension's manifest. This is required for component extensions so
    // that ExtensionService doesn't need to go to disk to load them.
    std::string manifest;

    // Directory where the extension is stored.
    FilePath root_directory;
  };

  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

  // If auto-updates are turned on, default to running every 5 hours.
  static const int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

  // The name of the file that the current active version number is stored in.
  static const char* kCurrentVersionFileName;

  // Determine if a given extension download should be treated as if it came
  // from the gallery. Note that this is requires *both* that the download_url
  // match and that the download was referred from a gallery page.
  bool IsDownloadFromGallery(const GURL& download_url,
                             const GURL& referrer_url);

  // Determine if the downloaded extension came from the theme mini-gallery,
  // Used to test if we need to show the "Loading" dialog for themes.
  static bool IsDownloadFromMiniGallery(const GURL& download_url);

  // Returns whether the URL is from either a hosted or packaged app.
  bool IsInstalledApp(const GURL& url);

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
                   bool autoupdate_enabled);

  // Gets the list of currently installed extensions.
  virtual const ExtensionList* extensions() const;
  virtual const ExtensionList* disabled_extensions() const;
  virtual const ExtensionList* terminated_extensions() const;

  // Gets the set of pending extensions.
  virtual const PendingExtensionMap& pending_extensions() const;

  // Registers an extension to be loaded as a component extension.
  void register_component_extension(const ComponentExtensionInfo& info) {
    component_extension_manifests_.push_back(info);
  }

  // Returns true if any extensions are installed.
  virtual bool HasInstalledExtensions();

  const FilePath& install_directory() const { return install_directory_; }

  DefaultApps* default_apps() { return &default_apps_; }

  // Whether this extension can run in an incognito window.
  bool IsIncognitoEnabled(const Extension* extension);
  void SetIsIncognitoEnabled(const Extension* extension, bool enabled);

  // Returns true if the given extension can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  bool CanCrossIncognito(const Extension* extension);

  // Whether this extension can inject scripts into pages with file URLs.
  bool AllowFileAccess(const Extension* extension);
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

  // Getter for the extension's runtime data PropertyBag.
  PropertyBag* GetPropertyBag(const Extension* extension);

  // Initialize and start all installed extensions.
  void Init();

  // Start up the extension event routers.
  void InitEventRouters();

  // Look up an extension by ID.
  virtual const Extension* GetExtensionById(const std::string& id,
                                            bool include_disabled);

  // Install the extension file at |extension_path|.  Will install as an
  // update if an older version is already installed.
  // For fresh installs, this method also causes the extension to be
  // immediately loaded.
  // TODO(aa): This method can be removed. It is only used by the unit tests,
  // and they could use CrxInstaller directly instead.
  void InstallExtension(const FilePath& extension_path);

  // Updates a currently-installed extension with the contents from
  // |extension_path|.
  // TODO(aa): This method can be removed. ExtensionUpdater could use
  // CrxInstaller directly instead.
  virtual void UpdateExtension(const std::string& id,
                               const FilePath& extension_path,
                               const GURL& download_url);

  // Adds an extension in a pending state; the extension with the
  // given info will be installed on the next auto-update cycle.
  //
  // It is an error to call this with an already-installed extension
  // (even a disabled one).
  //
  // TODO(akalin): Replace |install_silently| with a list of
  // pre-enabled permissions.
  void AddPendingExtensionFromSync(
      const std::string& id, const GURL& update_url,
      ShouldInstallExtensionPredicate should_install_extension,
      bool install_silently, bool enable_on_install,
      bool enable_incognito_on_install);

  // Given an extension id and an update URL, schedule the extension
  // to be fetched, installed, and activated.
  void AddPendingExtensionFromExternalUpdateUrl(const std::string& id,
                                                const GURL& update_url,
                                                Extension::Location location);

  // Like the above. Always installed silently, and defaults update url
  // from extension id.
  void AddPendingExtensionFromDefaultAppList(const std::string& id);

  // Reloads the specified extension.
  void ReloadExtension(const std::string& extension_id);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist. |external_uninstall| is a magical parameter
  // that is only used to send information to ExtensionPrefs, which external
  // callers should never set to true.
  // TODO(aa): Remove |external_uninstall| -- this information should be passed
  // to ExtensionPrefs some other way.
  void UninstallExtension(const std::string& extension_id,
                          bool external_uninstall);

  // Enable or disable an extension. No action if the extension is already
  // enabled/disabled.
  void EnableExtension(const std::string& extension_id);
  void DisableExtension(const std::string& extension_id);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extension|'s manifest.
  void GrantPermissions(const Extension* extension);

  // Updates the |extension|'s granted permissions lists to include all
  // permissions in the |extension|'s manifest and re-enables the
  // extension.
  void GrantPermissionsAndEnableExtension(const Extension* extension);

  // Load the extension from the directory |extension_path|.
  void LoadExtension(const FilePath& extension_path);

  // Load any component extensions.
  void LoadComponentExtensions();

  // Load all known extensions (used by startup and testing code).
  void LoadAllExtensions();

  // Continues loading all know extensions. It can be called from
  // LoadAllExtensions or from file thread if we had to relocalize manifest
  // (write_to_prefs is true in that case).
  void ContinueLoadAllExtensions(ExtensionPrefs::ExtensionsInfo* info,
                                 base::TimeTicks start_time,
                                 bool write_to_prefs);

  // Check for updates (or potentially new extensions from external providers)
  void CheckForExternalUpdates();

  // Unload the specified extension.
  void UnloadExtension(const std::string& extension_id,
                       UnloadedExtensionInfo::Reason reason);

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

  // If there is an extension for the specified url it is returned. Otherwise
  // returns the extension whose web extent contains |url|.
  const Extension* GetExtensionByWebExtent(const GURL& url);

  // Returns an extension that contains any URL that overlaps with the given
  // extent, if one exists.
  const Extension* GetExtensionByOverlappingWebExtent(
      const ExtensionExtent& extent);

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

  // Called when an extension has been loaded.
  void OnExtensionLoaded(const Extension* extension);

  // Called by the backend when an extension has been installed.
  void OnExtensionInstalled(const Extension* extension);

  // Checks if the privileges requested by |extension| have increased, and if
  // so, disables the extension and prompts the user to approve the change.
  void DisableIfPrivilegeIncrease(const Extension* extension);

  // Go through each extensions in pref, unload blacklisted extensions
  // and update the blacklist state in pref.
  virtual void UpdateExtensionBlacklist(
    const std::vector<std::string>& blacklist);

  // Go through each extension and unload those that the network admin has
  // put on the blacklist (not to be confused with the Google managed blacklist
  // set of extensions.
  virtual void CheckAdminBlacklist();

  void set_extensions_enabled(bool enabled) { extensions_enabled_ = enabled; }
  bool extensions_enabled() { return extensions_enabled_; }

  void set_show_extensions_prompts(bool enabled) {
    show_extensions_prompts_ = enabled;
  }

  bool show_extensions_prompts() {
    return show_extensions_prompts_;
  }

  virtual Profile* profile();

  // Profile calls this when it is being destroyed so that we know not to call
  // it.
  void DestroyingProfile();

  virtual ExtensionPrefs* extension_prefs();

  // Whether the extension service is ready.
  // TODO(skerner): Get rid of this method.  crbug.com/63756
  bool is_ready() { return ready_; }

  // Note that this may return NULL if autoupdate is not turned on.
  ExtensionUpdater* updater() { return updater_.get(); }

  ExtensionToolbarModel* toolbar_model() { return &toolbar_model_; }

  ExtensionsQuotaService* quota_service() { return &quota_service_; }

  ExtensionMenuManager* menu_manager() { return &menu_manager_; }

  ExtensionBrowserEventRouter* browser_event_router() {
    return browser_event_router_.get();
  }

  const std::map<GURL, int>& protected_storage_map() const {
    return protected_storage_map_;
  }

  // Notify the frontend that there was an error loading an extension.
  // This method is public because ExtensionServiceBackend can post to here.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error,
                                NotificationType type,
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
                                            Extension::Location location);

  virtual void OnExternalExtensionUpdateUrlFound(const std::string& id,
                                                 const GURL& update_url,
                                                 Extension::Location location);

  virtual void OnExternalProviderReady();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Whether there are any apps installed. Component apps are not included.
  bool HasApps() const;

  // Gets the set of loaded app ids. Component apps are not included.
  ExtensionIdSet GetAppIds() const;

 private:
  friend class BrowserThread;
  friend class DeleteTask<ExtensionService>;

  // Contains Extension data that can change during the life of the process,
  // but does not persist across restarts.
  struct ExtensionRuntimeData {
    // True if the background page is ready.
    bool background_page_ready;

    // True while the extension is being upgraded.
    bool being_upgraded;

    // Generic bag of runtime data that users can associate with extensions.
    PropertyBag property_bag;

    ExtensionRuntimeData();
    ~ExtensionRuntimeData();
  };
  typedef std::map<std::string, ExtensionRuntimeData> ExtensionRuntimeDataMap;

  virtual ~ExtensionService();

  // Clear all persistent data that may have been stored by the extension.
  void ClearExtensionData(const GURL& extension_url);

  // Look up an extension by ID, optionally including either or both of enabled
  // and disabled extensions.
  const Extension* GetExtensionByIdInternal(const std::string& id,
                                            bool include_enabled,
                                            bool include_disabled);


  // Keep track of terminated extensions.
  void TrackTerminatedExtension(const Extension* extension);
  void UntrackTerminatedExtension(const std::string& id);

  // Like AddPendingExtension*() functions above, but assumes an
  // extension with the same id is not already installed.
  void AddPendingExtensionInternal(
      const std::string& id, const GURL& update_url,
      ShouldInstallExtensionPredicate should_install_extension,
      bool is_from_sync, bool install_silently,
      bool enable_on_install, bool enable_incognito_on_install,
      Extension::Location install_source);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(const Extension* extension,
                               UnloadedExtensionInfo::Reason reason);

  // Helper that updates the active extension list used for crash reporting.
  void UpdateActiveExtensionsInCrashReporter();

  // Helper method. Loads extension from prefs.
  void LoadInstalledExtension(const ExtensionInfo& info, bool write_to_prefs);

  // Helper methods to configure the storage services accordingly.
  void GrantProtectedStorage(const Extension* extension);
  void RevokeProtectedStorage(const Extension* extension);
  void GrantUnlimitedStorage(const Extension* extension);
  void RevokeUnlimitedStorage(const Extension* extension);

  // The profile this ExtensionService is part of.
  Profile* profile_;

  // Preferences for the owning profile (weak reference).
  ExtensionPrefs* extension_prefs_;

  // The current list of installed extensions.
  // TODO(aa): This should use chrome/common/extensions/extension_set.h.
  ExtensionList extensions_;

  // The list of installed extensions that have been disabled.
  ExtensionList disabled_extensions_;

  // The list of installed extensions that have been terminated.
  ExtensionList terminated_extensions_;

  // Used to quickly check if an extension was terminated.
  std::set<std::string> terminated_extension_ids_;

  // The set of pending extensions.
  PendingExtensionMap pending_extensions_;

  // The map of extension IDs to their runtime data.
  ExtensionRuntimeDataMap extension_runtime_data_;

  // The full path to the directory where extensions are installed.
  FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_;

  // Whether to notify users when they attempt to install an extension.
  bool show_extensions_prompts_;

  // The backend that will do IO on behalf of this instance.
  scoped_refptr<ExtensionServiceBackend> backend_;

  // Used by dispatchers to limit API quota for individual extensions.
  ExtensionsQuotaService quota_service_;

  // Record that Init() has been called, and NotificationType::EXTENSIONS_READY
  // has fired.
  bool ready_;

  // Our extension updater, if updates are turned on.
  scoped_refptr<ExtensionUpdater> updater_;

  // The model that tracks extensions with BrowserAction buttons.
  ExtensionToolbarModel toolbar_model_;

  // Map unloaded extensions' ids to their paths. When a temporarily loaded
  // extension is unloaded, we lose the infomation about it and don't have
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

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Keeps track of menu items added by extensions.
  ExtensionMenuManager menu_manager_;

  // Keeps track of favicon-sized omnibox icons for extensions.
  ExtensionIconManager omnibox_icon_manager_;
  ExtensionIconManager omnibox_popup_icon_manager_;

  // List of registered component extensions (see Extension::Location).
  typedef std::vector<ComponentExtensionInfo> RegisteredComponentExtensions;
  RegisteredComponentExtensions component_extension_manifests_;

  // Collection of origins we've granted unlimited storage to. This is a
  // map from origin to the number of extensions requiring unlimited
  // storage within that origin.
  typedef std::map<GURL, int> UnlimitedStorageMap;
  UnlimitedStorageMap unlimited_storage_map_;

  // Collection of origins whose storage is protected by "Clear browsing data."
  // A map from origin to the number of Apps currently installed and therefore
  // intrinsically protected.
  typedef std::map<GURL, int> ProtectedStorageMap;
  ProtectedStorageMap protected_storage_map_;

  // Manages the installation of default apps and the promotion of them in the
  // app launcher.
  DefaultApps default_apps_;

  // Flag to make sure event routers are only initialized once.
  bool event_routers_initialized_;

  scoped_ptr<ExtensionBrowserEventRouter> browser_event_router_;

  // A collection of external extension providers.  Each provider reads
  // a source of external extension information.  Examples include the
  // windows registry and external_extensions.json.
  ProviderCollection external_extension_providers_;

  // Set to true by OnExternalExtensionUpdateUrlFound() when an external
  // extension URL is found.  Used in CheckForExternalUpdates() to see
  // if an update check is needed to install pending extensions.
  bool external_extension_url_added_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           UpdatePendingExtensionAlreadyInstalled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           InstallAppsWithUnlimtedStorage);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           InstallAppsAndCheckStorageProtection);
  DISALLOW_COPY_AND_ASSIGN(ExtensionService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_H_
