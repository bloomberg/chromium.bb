// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

// Class for managing global and per-extension preferences.
// This class is instantiated by ExtensionsService, so it should be accessed
// from there.
class ExtensionPrefs {
 public:
  typedef std::vector<linked_ptr<ExtensionInfo> > ExtensionsInfo;

  explicit ExtensionPrefs(PrefService* prefs, const FilePath& root_dir_);

  // Returns a copy of the Extensions prefs.
  // TODO(erikkay) Remove this so that external consumers don't need to be
  // aware of the internal structure of the preferences.
  DictionaryValue* CopyCurrentExtensions();

  // Populate |killed_ids| with extension ids that have been killed.
  void GetKilledExtensionIds(std::set<std::string>* killed_ids);

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
  void OnExtensionInstalled(Extension* extension,
                            Extension::State initial_state,
                            bool initial_incognito_enabled);

  // Called when an extension is uninstalled, so that prefs get cleaned up.
  void OnExtensionUninstalled(const std::string& extension_id,
                              const Extension::Location& location,
                              bool external_uninstall);

  // Returns the state (enabled/disabled) of the given extension.
  Extension::State GetExtensionState(const std::string& extension_id);

  // Called to change the extension's state when it is enabled/disabled.
  void SetExtensionState(Extension* extension, Extension::State);

  // Did the extension ask to escalate its permission during an upgrade?
  bool DidExtensionEscalatePermissions(const std::string& id);

  // If |did_escalate| is true, the preferences for |extension| will be set to
  // require the install warning when the user tries to enable.
  void SetDidExtensionEscalatePermissions(Extension* extension,
                                          bool did_escalate);

  // Returns the version string for the currently installed extension, or
  // the empty string if not found.
  std::string GetVersionString(const std::string& extension_id);

  // Re-writes the extension manifest into the prefs.
  // Called to change the extension's manifest when it's re-localized.
  void UpdateManifest(Extension* extension);

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

  static void RegisterUserPrefs(PrefService* prefs);

  // The underlying PrefService.
  PrefService* pref_service() const { return prefs_; }

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
  // Return false if the value is false or kPrefBlacklist does not exist.
  bool ReadBooleanFromPref(DictionaryValue* ext, const std::string& pref_key);

  // Reads a boolean pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefBoolean(const std::string& extension_id,
                                const std::string& pref_key);

  // Ensures and returns a mutable dictionary for extension |id|'s prefs.
  DictionaryValue* GetOrCreateExtensionPref(const std::string& id);

  // Same as above, but returns NULL if it doesn't exist.
  DictionaryValue* GetExtensionPref(const std::string& id) const;

  // Checks if kPrefBlacklist is set to true in the DictionaryValue.
  // Return false if the value is false or kPrefBlacklist does not exist.
  // This is used to decide if an extension is blacklisted.
  bool IsBlacklistBitSet(DictionaryValue* ext);

  // Helper methods for the public last ping day functions.
  base::Time LastPingDayImpl(const DictionaryValue* dictionary) const;
  void SetLastPingDayImpl(const base::Time& time, DictionaryValue* dictionary);

  // The pref service specific to this set of extension prefs.
  PrefService* prefs_;

  // Base extensions install directory.
  FilePath install_directory_;

  // The URLs of all of the toolstrips.
  URLList shelf_order_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefs);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
