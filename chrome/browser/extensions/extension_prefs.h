// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_content_settings_store.h"
#include "chrome/browser/extensions/extension_prefs_scope.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/string_ordinal.h"
#include "googleurl/src/gurl.h"

class ExtensionPrefValueMap;
class URLPatternSet;

// Class for managing global and per-extension preferences.
//
// This class distinguishes the following kinds of preferences:
// - global preferences:
//       internal state for the extension system in general, not associated
//       with an individual extension, such as lastUpdateTime.
// - per-extension preferences:
//       meta-preferences describing properties of the extension like
//       installation time, whether the extension is enabled, etc.
// - extension controlled preferences:
//       browser preferences that an extension controls. For example, an
//       extension could use the proxy API to specify the browser's proxy
//       preference. Extension-controlled preferences are stored in
//       PrefValueStore::extension_prefs(), which this class populates and
//       maintains as the underlying extensions change.
class ExtensionPrefs : public ExtensionContentSettingsStore::Observer {
 public:
  // Key name for a preference that keeps track of per-extension settings. This
  // is a dictionary object read from the Preferences file, keyed off of
  // extension ids.
  static const char kExtensionsPref[];

  typedef std::vector<linked_ptr<ExtensionInfo> > ExtensionsInfo;

  // Vector containing identifiers for preferences.
  typedef std::set<std::string> PrefKeySet;

  // Vector containing identifiers for extensions.
  typedef std::vector<std::string> ExtensionIdSet;

  // This enum is used for the launch type the user wants to use for an
  // application.
  // Do not remove items or re-order this enum as it is used in preferences
  // and histograms.
  enum LaunchType {
    LAUNCH_PINNED,
    LAUNCH_REGULAR,
    LAUNCH_FULLSCREEN,
    LAUNCH_WINDOW,

    // Launch an app in the in the way a click on the NTP would,
    // if no user pref were set.  Update this constant to change
    // the default for the NTP and chrome.management.launchApp().
    LAUNCH_DEFAULT = LAUNCH_REGULAR
  };

  // Does not assume ownership of |prefs| and |extension_pref_value_map|.
  // Note that you must call Init() to finalize construction.
  ExtensionPrefs(PrefService* prefs,
                 const FilePath& root_dir,
                 ExtensionPrefValueMap* extension_pref_value_map);
  virtual ~ExtensionPrefs();

  // If |extensions_disabled| is true, extension controlled preferences and
  // content settings do not become effective.
  void Init(bool extensions_disabled);

  // Returns a copy of the Extensions prefs.
  // TODO(erikkay) Remove this so that external consumers don't need to be
  // aware of the internal structure of the preferences.
  base::DictionaryValue* CopyCurrentExtensions();

  // Returns true if the specified external extension was uninstalled by the
  // user.
  bool IsExternalExtensionUninstalled(const std::string& id) const;

  // Checks whether |extension_id| is disabled. If there's no state pref for
  // the extension, this will return false. Generally you should use
  // ExtensionService::IsExtensionEnabled instead.
  bool IsExtensionDisabled(const std::string& id) const;

  // Get the order that the browser actions appear in the toolbar.
  std::vector<std::string> GetToolbarOrder();

  // Set the order that the browser actions appear in the toolbar.
  void SetToolbarOrder(const std::vector<std::string>& extension_ids);

  // Called when an extension is installed, so that prefs get created.
  // If |page_ordinal| is an invalid ordinal, then a page will be found
  // for the App.
  void OnExtensionInstalled(const Extension* extension,
                            Extension::State initial_state,
                            bool from_webstore,
                            const StringOrdinal& page_ordinal);

  // Called when an extension is uninstalled, so that prefs get cleaned up.
  void OnExtensionUninstalled(const std::string& extension_id,
                              const Extension::Location& location,
                              bool external_uninstall);

  // Updates the app launcher value for the moved extension so that it is now
  // located after the given predecessor and before the successor.
  // Empty strings are used to indicate no successor or predecessor.
  void OnExtensionMoved(const std::string& moved_extension_id,
                        const std::string& predecessor_extension_id,
                        const std::string& successor_extension_id);

  // Called to change the extension's state when it is enabled/disabled.
  void SetExtensionState(const std::string& extension_id, Extension::State);

  // Returns all installed extensions
  void GetExtensions(ExtensionIdSet* out);

  // Getter and setter for browser action visibility.
  bool GetBrowserActionVisibility(const Extension* extension);
  void SetBrowserActionVisibility(const Extension* extension, bool visible);

  // Did the extension ask to escalate its permission during an upgrade?
  bool DidExtensionEscalatePermissions(const std::string& id);

  // If |did_escalate| is true, the preferences for |extension| will be set to
  // require the install warning when the user tries to enable.
  void SetDidExtensionEscalatePermissions(const Extension* extension,
                                          bool did_escalate);

  // Returns the version string for the currently installed extension, or
  // the empty string if not found.
  std::string GetVersionString(const std::string& extension_id);

  // Re-writes the extension manifest into the prefs.
  // Called to change the extension's manifest when it's re-localized.
  void UpdateManifest(const Extension* extension);

  // Returns extension path based on extension ID, or empty FilePath on error.
  FilePath GetExtensionPath(const std::string& extension_id);

  // Returns base extensions install directory.
  const FilePath& install_directory() const { return install_directory_; }

  // Updates the prefs based on the blacklist.
  void UpdateBlacklist(const std::set<std::string>& blacklist_set);

  // Based on extension id, checks prefs to see if it is blacklisted.
  bool IsExtensionBlacklisted(const std::string& id);

  // Based on extension id, checks prefs to see if it is orphaned.
  bool IsExtensionOrphaned(const std::string& id);

  // Whether the user has acknowledged an external extension.
  bool IsExternalExtensionAcknowledged(const std::string& extension_id);
  void AcknowledgeExternalExtension(const std::string& extension_id);

  // Whether the user has acknowledged a blacklisted extension.
  bool IsBlacklistedExtensionAcknowledged(const std::string& extension_id);
  void AcknowledgeBlacklistedExtension(const std::string& extension_id);

  // Whether the user has acknowledged an orphaned extension.
  bool IsOrphanedExtensionAcknowledged(const std::string& extension_id);
  void AcknowledgeOrphanedExtension(const std::string& extension_id);

  // Returns true if the extension notification code has already run for the
  // first time for this profile. Currently we use this flag to mean that any
  // extensions that would trigger notifications should get silently
  // acknowledged. This is a fuse. Calling it the first time returns false.
  // Subsequent calls return true. It's not possible through an API to ever
  // reset it. Don't call it unless you mean it!
  bool SetAlertSystemFirstRun();

  // The oauth client id used for app notification setup, if any.
  std::string GetAppNotificationClientId(const std::string& extension_id) const;
  void SetAppNotificationClientId(const std::string& extension_id,
                                  const std::string& oauth_client_id);

  // Whether app notifications are disabled for the given app.
  bool IsAppNotificationDisabled(const std::string& extension_id) const;
  void SetAppNotificationDisabled(const std::string& extension_id, bool value);

  // Is the extension with |extension_id| allowed by policy (checking both
  // whitelist and blacklist).
  bool IsExtensionAllowedByPolicy(const std::string& extension_id,
                                  Extension::Location location);

  // Returns the last value set via SetLastPingDay. If there isn't such a
  // pref, the returned Time will return true for is_null().
  base::Time LastPingDay(const std::string& extension_id) const;

  // The time stored is based on the server's perspective of day start time, not
  // the client's.
  void SetLastPingDay(const std::string& extension_id, const base::Time& time);

  // Similar to the 2 above, but for the extensions blacklist.
  base::Time BlacklistLastPingDay() const;
  void SetBlacklistLastPingDay(const base::Time& time);

  // Similar to LastPingDay/SetLastPingDay, but for sending "days since active"
  // ping.
  base::Time LastActivePingDay(const std::string& extension_id);
  void SetLastActivePingDay(const std::string& extension_id,
                            const base::Time& time);

  // A bit we use for determining if we should send the "days since active"
  // ping. A value of true means the item has been active (launched) since the
  // last update check.
  bool GetActiveBit(const std::string& extension_id);
  void SetActiveBit(const std::string& extension_id, bool active);

  // Returns the granted permission set for the extension with |extension_id|,
  // and NULL if no preferences were found for |extension_id|.
  // This passes ownership of the returned set to the caller.
  ExtensionPermissionSet* GetGrantedPermissions(
      const std::string& extension_id);

  // Adds |permissions| to the granted permissions set for the extension with
  // |extension_id|. The new granted permissions set will be the union of
  // |permissions| and the already granted permissions.
  void AddGrantedPermissions(const std::string& extension_id,
                             const ExtensionPermissionSet* permissions);

  // Gets the active permission set for the specified extension. This may
  // differ from the permissions in the manifest due to the optional
  // permissions API. This passes ownership of the set to the caller.
  ExtensionPermissionSet* GetActivePermissions(
      const std::string& extension_id);

  // Sets the active |permissions| for the extension with |extension_id|.
  void SetActivePermissions(const std::string& extension_id,
                            const ExtensionPermissionSet* permissions);

  // Returns true if the user enabled this extension to be loaded in incognito
  // mode.
  bool IsIncognitoEnabled(const std::string& extension_id);
  void SetIsIncognitoEnabled(const std::string& extension_id, bool enabled);

  // Returns true if the user has chosen to allow this extension to inject
  // scripts into pages with file URLs.
  bool AllowFileAccess(const std::string& extension_id);
  void SetAllowFileAccess(const std::string& extension_id, bool allow);
  bool HasAllowFileAccessSetting(const std::string& extension_id) const;

  // Sets the extension preference indicating that an extension wants to delay
  // network requests on browser startup.
  void SetDelaysNetworkRequests(const std::string& extension_id,
                                bool does_delay);

  // Returns true if an extension has registered to delay network requests on
  // browser startup.
  bool DelaysNetworkRequests(const std::string& extension_id);

  // Get the launch type preference.  If no preference is set, return
  // |default_pref_value|.
  LaunchType GetLaunchType(const std::string& extension_id,
                           LaunchType default_pref_value);

  void SetLaunchType(const std::string& extension_id, LaunchType launch_type);

  // Find the right launch container based on the launch type.
  // If |extension|'s prefs do not have a launch type set, then
  // use |default_pref_value|.
  extension_misc::LaunchContainer GetLaunchContainer(
      const Extension* extension,
      LaunchType default_pref_value);

  // Saves ExtensionInfo for each installed extension with the path to the
  // version directory and the location. Blacklisted extensions won't be saved
  // and neither will external extensions the user has explicitly uninstalled.
  // Caller takes ownership of returned structure.
  ExtensionsInfo* GetInstalledExtensionsInfo();

  // Returns the ExtensionInfo from the prefs for the given extension. If the
  // extension is not present, NULL is returned.
  ExtensionInfo* GetInstalledExtensionInfo(const std::string& extension_id);

  // We've downloaded an updated .crx file for the extension, but are waiting
  // for idle time to install it.
  void SetIdleInstallInfo(const std::string& extension_id,
                          const FilePath& crx_path,
                          const std::string& version,
                          const base::Time& fetch_time);

  // Removes any idle install information we have for the given |extension_id|.
  // Returns true if there was info to remove; false otherwise.
  bool RemoveIdleInstallInfo(const std::string& extension_id);

  // If we have idle install information for |extension_id|, this puts it into
  // the out parameters and returns true. Otherwise returns false.
  bool GetIdleInstallInfo(const std::string& extension_id,
                          FilePath* crx_path,
                          std::string* version,
                          base::Time* fetch_time);

  // Returns the extension id's that have idle install information.
  std::set<std::string> GetIdleInstallInfoIds();

  // We allow the web store to set a string containing login information when a
  // purchase is made, so that when a user logs into sync with a different
  // account we can recognize the situation. The Get function returns true if
  // there was previously stored data (placing it in |result|), or false
  // otherwise. The Set will overwrite any previous login.
  bool GetWebStoreLogin(std::string* result);
  void SetWebStoreLogin(const std::string& login);

  // Get the application launch ordinal for an app with |extension_id|. This
  // determines the order in which the app appears on the page it's on in the
  // New Tab Page (Note that you can compare app launch ordinals only if the
  // apps are on the same page). A string value close to |a*| generally
  // indicates top left. If the extension has no launch ordinal, an invalid
  // StringOrdinal is returned.
  StringOrdinal GetAppLaunchOrdinal(const std::string& extension_id) const;

  // Sets a specific launch ordinal for an app with |extension_id|.
  void SetAppLaunchOrdinal(const std::string& extension_id,
                           const StringOrdinal& ordinal);

  // Returns a StringOrdinal that is lower than any app launch ordinal for the
  // given page.
  StringOrdinal CreateFirstAppLaunchOrdinal(const StringOrdinal& page_ordinal)
      const;

  // Returns a StringOrdinal that is higher than any app launch ordinal for the
  // given page.
  StringOrdinal CreateNextAppLaunchOrdinal(const StringOrdinal& page_ordinal)
      const;

  // Returns a StringOrdinal that is lower than any existing page ordinal.
  StringOrdinal CreateFirstAppPageOrdinal() const;

  // Gets the page a new app should install to, which is the earliest non-full
  // page.  The returned ordinal may correspond to a page that doesn't yet exist
  // if all pages are full.
  StringOrdinal GetNaturalAppPageOrdinal() const;

  // Get the page ordinal for an app with |extension_id|. This determines
  // which page an app will appear on in page-based NTPs.  If the app has no
  // page specified, an invalid StringOrdinal is returned.
  StringOrdinal GetPageOrdinal(const std::string& extension_id) const;

  // Sets a specific page ordinal for an app with |extension_id|.
  void SetPageOrdinal(const std::string& extension_id,
                      const StringOrdinal& ordinal);

  // Removes the page ordinal for an app.
  void ClearPageOrdinal(const std::string& extension_id);

  // Convert the page StringOrdinal value to its integer equivalent. This takes
  // O(# of apps) worst-case.
  int PageStringOrdinalAsInteger(const StringOrdinal& page_ordinal) const;

  // Converts the page index integer to its StringOrdinal equivalent. This takes
  // O(# of apps) worst-case.
  StringOrdinal PageIntegerAsStringOrdinal(size_t page_index) const;

  // Returns true if the user repositioned the app on the app launcher via drag
  // and drop.
  bool WasAppDraggedByUser(const std::string& extension_id);

  // Sets a flag indicating that the user repositioned the app on the app
  // launcher by drag and dropping it.
  void SetAppDraggedByUser(const std::string& extension_id);

  // The extension's update URL data.  If not empty, the ExtensionUpdater
  // will append a ap= parameter to the URL when checking if a new version
  // of the extension is available.
  void SetUpdateUrlData(const std::string& extension_id,
                        const std::string& data);
  std::string GetUpdateUrlData(const std::string& extension_id);

  // Sets a preference value that is controlled by the extension. In other
  // words, this is not a pref value *about* the extension but something
  // global the extension wants to override.
  // Takes ownership of |value|.
  void SetExtensionControlledPref(const std::string& extension_id,
                                  const std::string& pref_key,
                                  ExtensionPrefsScope scope,
                                  base::Value* value);

  void RemoveExtensionControlledPref(const std::string& extension_id,
                                     const std::string& pref_key,
                                     ExtensionPrefsScope scope);

  // Returns true if currently no extension with higher precedence controls the
  // preference.
  bool CanExtensionControlPref(const std::string& extension_id,
                               const std::string& pref_key,
                               bool incognito);

  // Returns true if extension |extension_id| currently controls the
  // preference.
  bool DoesExtensionControlPref(const std::string& extension_id,
                                const std::string& pref_key,
                                bool incognito);

  // Returns true if there is an extension which controls the preference value
  //  for |pref_key| *and* it is specific to incognito mode.
  bool HasIncognitoPrefValue(const std::string& pref_key);

  // Clears incognito session-only content settings for all extensions.
  void ClearIncognitoSessionOnlyContentSettings();

  // Returns true if the extension was installed from the Chrome Web Store.
  bool IsFromWebStore(const std::string& extension_id) const;

  // Returns true if the extension was installed from an App generated from a
  // bookmark.
  bool IsFromBookmark(const std::string& extension_id) const;

  // Helper method to acquire the installation time of an extension.
  // Returns base::Time() if the installation time could not be parsed or
  // found.
  base::Time GetInstallTime(const std::string& extension_id) const;

  static void RegisterUserPrefs(PrefService* prefs);

  ExtensionContentSettingsStore* content_settings_store() {
    return content_settings_store_.get();
  }

  // The underlying PrefService.
  PrefService* pref_service() const { return prefs_; }

 protected:
  // For unit testing. Enables injecting an artificial clock that is used
  // to query the current time, when an extension is installed.
  virtual base::Time GetCurrentTime() const;

 private:
  friend class ExtensionPrefsGetMinOrMaxAppLaunchOrdinalsOnPage;  // Unit test.
  friend class ExtensionPrefsMigrateAppIndex;  // Unit test.
  friend class ExtensionPrefsMigrateAppIndexInvalid;  // Unit test.
  friend class ExtensionPrefsUninstallExtension;  // Unit test.

  // ExtensionContentSettingsStore::Observer methods:
  virtual void OnContentSettingChanged(
      const std::string& extension_id,
      bool incognito) OVERRIDE;

  // Converts absolute paths in the pref to paths relative to the
  // install_directory_.
  void MakePathsRelative();

  // Converts internal relative paths to be absolute. Used for export to
  // consumers who expect full paths.
  void MakePathsAbsolute(base::DictionaryValue* dict);

  // Sets the pref |key| for extension |id| to |value|.
  void UpdateExtensionPref(const std::string& id,
                           const std::string& key,
                           base::Value* value);

  // Deletes the pref dictionary for extension |id|.
  void DeleteExtensionPrefs(const std::string& id);

  // Reads a boolean pref from |ext| with key |pref_key|.
  // Return false if the value is false or |pref_key| does not exist.
  static bool ReadBooleanFromPref(const base::DictionaryValue* ext,
                                  const std::string& pref_key);

  // Reads a boolean pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefBoolean(const std::string& extension_id,
                                const std::string& pref_key) const;

  // Reads an integer pref from |ext| with key |pref_key|.
  // Return false if the value does not exist.
  static bool ReadIntegerFromPref(const base::DictionaryValue* ext,
                                  const std::string& pref_key,
                                  int* out_value);

  // Reads an integer pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefInteger(const std::string& extension_id,
                                const std::string& pref_key,
                                int* out_value);

  // Reads a list pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefList(const std::string& extension_id,
                             const std::string& pref_key,
                             const base::ListValue** out_value);

  // Reads a string pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefString(const std::string& extension_id,
                               const std::string& pref_key,
                               std::string* out_value) const;

  // Interprets the list pref, |pref_key| in |extension_id|'s preferences, as a
  // URLPatternSet. The |valid_schemes| specify how to parse the URLPatterns.
  bool ReadExtensionPrefURLPatternSet(const std::string& extension_id,
                                      const std::string& pref_key,
                                      URLPatternSet* result,
                                      int valid_schemes);

  // Converts |new_value| to a list of strings and sets the |pref_key| pref
  // belonging to |extension_id|.
  void SetExtensionPrefURLPatternSet(const std::string& extension_id,
                                     const std::string& pref_key,
                                     const URLPatternSet& new_value);

  // Interprets |pref_key| in |extension_id|'s preferences as an
  // ExtensionPermissionSet, and passes ownership of the set to the caller.
  ExtensionPermissionSet* ReadExtensionPrefPermissionSet(
      const std::string& extension_id,
      const std::string& pref_key);

  // Converts the |new_value| to its value and sets the |pref_key| pref
  // belonging to |extension_id|.
  void SetExtensionPrefPermissionSet(const std::string& extension_id,
                                     const std::string& pref_key,
                                     const ExtensionPermissionSet* new_value);

  // Returns a dictionary for extension |id|'s prefs or NULL if it doesn't
  // exist.
  const base::DictionaryValue* GetExtensionPref(const std::string& id) const;

  // Returns the dictionary of preferences controlled by the specified extension
  // or creates a new one. All entries in the dictionary contain non-expanded
  // paths.
  const base::DictionaryValue* GetExtensionControlledPrefs(
      const std::string& id,
      bool incognito) const;

  // Serializes the data and schedules a persistent save via the |PrefService|.
  // TODO(andybons): Fire an EXTENSION_PREF_CHANGED notification to be more
  // granular than PREF_CHANGED.
  void SavePrefs();

  // Checks if kPrefBlacklist is set to true in the DictionaryValue.
  // Return false if the value is false or kPrefBlacklist does not exist.
  // This is used to decide if an extension is blacklisted.
  static bool IsBlacklistBitSet(base::DictionaryValue* ext);

  // Fix missing preference entries in the extensions that are were introduced
  // in a later Chrome version.
  void FixMissingPrefs(const ExtensionIdSet& extension_ids);

  // Installs the persistent extension preferences into |prefs_|'s extension
  // pref store. Does nothing if |extensions_disabled| is true.
  void InitPrefStore(bool extensions_disabled);

  // Migrates the permissions data in the pref store.
  void MigratePermissions(const ExtensionIdSet& extension_ids);

  // Migrates the app launcher and page index values.
  void MigrateAppIndex(const ExtensionIdSet& extension_ids);

  // Checks whether there is a state pref for the extension and if so, whether
  // it matches |check_state|.
  bool DoesExtensionHaveState(const std::string& id,
                              Extension::State check_state) const;

  // An enum used by GetMinOrMaxAppLaunchOrdinalsOnPage to specify which
  // value should be returned.
  enum AppLaunchOrdinalReturn {MIN_ORDINAL, MAX_ORDINAL};

  // This function returns the lowest ordinal on |page_ordinal| if
  // |return_value| == AppLaunchOrdinalReturn::MIN_ORDINAL, otherwise it returns
  // the largest ordinal on |page_ordinal|. If there are no apps on the page
  // then an invalid StringOrdinal is returned. It is an error to call this
  // function with an invalid |page_ordinal|.
  StringOrdinal GetMinOrMaxAppLaunchOrdinalsOnPage(
      const StringOrdinal& page_ordinal,
      AppLaunchOrdinalReturn return_type) const;

  // Initialize the |page_ordinal_map_| with the page ordinals used by the
  // given extensions.
  void InitializePageOrdinalMap(const ExtensionIdSet& extension_ids);

  // Called when an application changes the value of its page ordinal so that
  // |page_ordinal_map_| is aware that |old_value| page ordinal has been
  // replace by the |new_value| page ordinal and adjusts its mapping
  // accordingly. This works with valid and invalid StringOrdinals.
  void UpdatePageOrdinalMap(const StringOrdinal& old_value,
                            const StringOrdinal& new_value);

  // The pref service specific to this set of extension prefs. Owned by profile.
  PrefService* prefs_;

  // Base extensions install directory.
  FilePath install_directory_;

  // A map of all the StringOrdinal page indices mapping to how often they are
  // used, this is used for mapping the StringOrdinals to their integer
  // equivalent as well as quick lookup of the sorted StringOrdinals.
  // TODO(csharp) Convert this to a two-layer map to allow page ordinal lookup
  // by id and vice versa.
  typedef std::map<StringOrdinal, int, StringOrdinalLessThan> PageOrdinalMap;
  PageOrdinalMap page_ordinal_map_;

  // Weak pointer, owned by Profile.
  ExtensionPrefValueMap* extension_pref_value_map_;

  scoped_refptr<ExtensionContentSettingsStore> content_settings_store_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefs);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
