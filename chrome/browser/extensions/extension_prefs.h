// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/linked_ptr.h"
#include "base/time.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

class ExtensionPrefValueMap;

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
class ExtensionPrefs {
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

  // Does not assume ownership of |prefs| and |incognito_prefs|.
  explicit ExtensionPrefs(PrefService* prefs,
                          const FilePath& root_dir,
                          ExtensionPrefValueMap* extension_pref_value_map);
  ~ExtensionPrefs();

  // Returns a copy of the Extensions prefs.
  // TODO(erikkay) Remove this so that external consumers don't need to be
  // aware of the internal structure of the preferences.
  DictionaryValue* CopyCurrentExtensions();

  // Returns true if the specified extension has an entry in prefs
  // and its killbit is on.
  bool IsExtensionKilled(const std::string& id);

  // Get the order that toolstrip URLs appear in the shelf.
  typedef std::vector<GURL> URLList;
  URLList GetShelfToolstripOrder();

  // Sets the order that toolstrip URLs appear in the shelf.
  void SetShelfToolstripOrder(const URLList& urls);

  // Get the order that the browser actions appear in the toolbar.
  std::vector<std::string> GetToolbarOrder();

  // Set the order that the browser actions appear in the toolbar.
  void SetToolbarOrder(const std::vector<std::string>& extension_ids);

  // Called when an extension is installed, so that prefs get created.
  void OnExtensionInstalled(const Extension* extension,
                            Extension::State initial_state,
                            bool initial_incognito_enabled);

  // Called when an extension is uninstalled, so that prefs get cleaned up.
  void OnExtensionUninstalled(const std::string& extension_id,
                              const Extension::Location& location,
                              bool external_uninstall);

  // Returns the state (enabled/disabled) of the given extension.
  Extension::State GetExtensionState(const std::string& extension_id) const;

  // Called to change the extension's state when it is enabled/disabled.
  void SetExtensionState(const Extension* extension, Extension::State);

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

  // Is the extension with |extension_id| allowed by policy (checking both
  // whitelist and blacklist).
  bool IsExtensionAllowedByPolicy(const std::string& extension_id);

  // Returns the last value set via SetLastPingDay. If there isn't such a
  // pref, the returned Time will return true for is_null().
  base::Time LastPingDay(const std::string& extension_id) const;

  // The time stored is based on the server's perspective of day start time, not
  // the client's.
  void SetLastPingDay(const std::string& extension_id, const base::Time& time);

  // Gets the permissions (|api_permissions|, |host_extent| and |full_access|)
  // granted to the extension with |extension_id|. |full_access| will be true
  // if the extension has all effective permissions (like from an NPAPI plugin).
  // Returns false if the granted permissions haven't been initialized yet.
  // TODO(jstritar): Refractor the permissions into a class that encapsulates
  // all granted permissions, can be initialized from preferences or
  // a manifest file, and can be compared to each other.
  bool GetGrantedPermissions(const std::string& extension_id,
                             bool* full_access,
                             std::set<std::string>* api_permissions,
                             ExtensionExtent* host_extent);

  // Adds the specified |api_permissions|, |host_extent| and |full_access|
  // to the granted permissions for extension with |extension_id|.
  // |full_access| should be set to true if the extension effectively has all
  // permissions (such as by having an NPAPI plugin).
  void AddGrantedPermissions(const std::string& extension_id,
                             const bool full_access,
                             const std::set<std::string>& api_permissions,
                             const ExtensionExtent& host_extent);

  // Similar to the 2 above, but for the extensions blacklist.
  base::Time BlacklistLastPingDay() const;
  void SetBlacklistLastPingDay(const base::Time& time);

  // Returns true if the user enabled this extension to be loaded in incognito
  // mode.
  bool IsIncognitoEnabled(const std::string& extension_id);
  void SetIsIncognitoEnabled(const std::string& extension_id, bool enabled);

  // Returns true if the user has chosen to allow this extension to inject
  // scripts into pages with file URLs.
  bool AllowFileAccess(const std::string& extension_id);
  void SetAllowFileAccess(const std::string& extension_id, bool allow);

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

  // Get the application launch index for an extension with |extension_id|. This
  // determines the order of which the applications appear on the New Tab Page.
  // A value of 0 generally indicates top left. If the extension has no launch
  // index a -1 value is returned.
  int GetAppLaunchIndex(const std::string& extension_id);

  // Sets a specific launch index for an extension with |extension_id|.
  void SetAppLaunchIndex(const std::string& extension_id, int index);

  // Gets the next available application launch index. This is 1 higher than the
  // highest current application launch index found.
  int GetNextAppLaunchIndex();

  // Sets the order the apps should be displayed in the app launcher.
  void SetAppLauncherOrder(const std::vector<std::string>& extension_ids);

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
                                  bool incognito,
                                  Value* value);

  void RemoveExtensionControlledPref(const std::string& extension_id,
                                     const std::string& pref_key,
                                     bool incognito);

  static void RegisterUserPrefs(PrefService* prefs);

  // The underlying PrefService.
  PrefService* pref_service() const { return prefs_; }

 protected:
  // For unit testing. Enables injecting an artificial clock that is used
  // to query the current time, when an extension is installed.
  virtual base::Time GetCurrentTime() const;

 private:
  // Converts absolute paths in the pref to paths relative to the
  // install_directory_.
  void MakePathsRelative();

  // Converts internal relative paths to be absolute. Used for export to
  // consumers who expect full paths.
  void MakePathsAbsolute(DictionaryValue* dict);

  // Sets the pref |key| for extension |id| to |value|.
  void UpdateExtensionPref(const std::string& id,
                           const std::string& key,
                           Value* value);

  // Deletes the pref dictionary for extension |id|.
  void DeleteExtensionPrefs(const std::string& id);

  // Reads a boolean pref from |ext| with key |pref_key|.
  // Return false if the value is false or |pref_key| does not exist.
  bool ReadBooleanFromPref(DictionaryValue* ext, const std::string& pref_key);

  // Reads a boolean pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefBoolean(const std::string& extension_id,
                                const std::string& pref_key);

  // Reads an integer pref from |ext| with key |pref_key|.
  // Return false if the value does not exist.
  bool ReadIntegerFromPref(DictionaryValue* ext, const std::string& pref_key,
                           int* out_value);

  // Reads an integer pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefInteger(const std::string& extension_id,
                                const std::string& pref_key,
                                int* out_value);

  // Reads a list pref |pref_key| from extension with id | extension_id|.
  bool ReadExtensionPrefList(const std::string& extension_id,
                             const std::string& pref_key,
                             ListValue** out_value);

  // Reads a list pref |pref_key| as a string set from the extension with
  // id |extension_id|.
  bool ReadExtensionPrefStringSet(const std::string& extension_id,
                                  const std::string& pref_key,
                                  std::set<std::string>* result);

  // Adds the |added_values| to the value of |pref_key| for the extension
  // with id |extension_id| (the new value will be the union of the existing
  // value and |added_values|).
  void AddToExtensionPrefStringSet(const std::string& extension_id,
                                   const std::string& pref_key,
                                   const std::set<std::string>& added_values);

  // Ensures and returns a mutable dictionary for extension |id|'s prefs.
  DictionaryValue* GetOrCreateExtensionPref(const std::string& id);

  // Same as above, but returns NULL if it doesn't exist.
  DictionaryValue* GetExtensionPref(const std::string& id) const;

  // Returns the dictionary of preferences controlled by the specified extension
  // or creates a new one. All entries in the dictionary contain non-expanded
  // paths.
  DictionaryValue* GetExtensionControlledPrefs(const std::string& id) const;

  // Serializes the data and schedules a persistent save via the |PrefService|.
  // Additionally fires a PREF_CHANGED notification with the top-level
  // |kExtensionsPref| path set.
  // TODO(andybons): Switch this to EXTENSION_PREF_CHANGED to be more granular.
  // TODO(andybons): Use a ScopedPrefUpdate to update observers on changes to
  // the mutable extension dictionary.
  void SavePrefsAndNotify();

  // Checks if kPrefBlacklist is set to true in the DictionaryValue.
  // Return false if the value is false or kPrefBlacklist does not exist.
  // This is used to decide if an extension is blacklisted.
  bool IsBlacklistBitSet(DictionaryValue* ext);

  // Helper methods for the public last ping day functions.
  base::Time LastPingDayImpl(const DictionaryValue* dictionary) const;
  void SetLastPingDayImpl(const base::Time& time, DictionaryValue* dictionary);

  // Helper method to acquire the installation time of an extension.
  // Returns base::Time() if the installation time could not be parsed or
  // found.
  base::Time GetInstallTime(const std::string& extension_id) const;

  // Fix missing preference entries in the extensions that are were introduced
  // in a later Chrome version.
  void FixMissingPrefs(const ExtensionIdSet& extension_ids);

  // Installs the persistent extension preferences into |prefs_|'s extension
  // pref store.
  void InitPrefStore();

  // The pref service specific to this set of extension prefs. Owned by profile.
  PrefService* prefs_;

  // Base extensions install directory.
  FilePath install_directory_;

  // Weak pointer, owned by Profile.
  ExtensionPrefValueMap* extension_pref_value_map_;

  // The URLs of all of the toolstrips.
  URLList shelf_order_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefs);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
