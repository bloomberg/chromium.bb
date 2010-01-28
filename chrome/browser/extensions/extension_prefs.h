// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_

#include <set>
#include <string>
#include <vector>

#include "base/linked_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_service.h"
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
  void OnExtensionInstalled(Extension* extension);

  // Called when an extension is uninstalled, so that prefs get cleaned up.
  void OnExtensionUninstalled(const Extension* extension,
                              bool external_uninstall);

  // Returns the state (enabled/disabled) of the given extension.
  Extension::State GetExtensionState(const std::string& extension_id);

  // Called to change the extension's state when it is enabled/disabled.
  void SetExtensionState(Extension* extension, Extension::State);

  // If |require| is true, the preferences for |extension| will be set to
  // require the install warning when the user tries to enable.
  void SetShowInstallWarningOnEnable(Extension* extension, bool require);

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

  // Did the extension ask to escalate its permission during an upgrade?
  bool DidExtensionEscalatePermissions(const std::string& id);

  // Returns the last value set via SetLastPingDay. If there isn't such a
  // pref, the returned Time will return true for is_null().
  base::Time LastPingDay(const std::string& extension_id);

  // The time stored is based on the server's perspective of day start time, not
  // the client's.
  void SetLastPingDay(const std::string& extension_id, const base::Time& time);


  // Saves ExtensionInfo for each installed extension with the path to the
  // version directory and the location. Blacklisted extensions won't be saved
  // and neither will external extensions the user has explicitly uninstalled.
  // Caller takes ownership of returned structure.
  static ExtensionsInfo* CollectExtensionsInfo(ExtensionPrefs* extension_prefs);

 private:

  // Converts absolute paths in the pref to paths relative to the
  // install_directory_.
  void MakePathsRelative();

  // Converts internal relative paths to be absolute. Used for export to
  // consumers who expect full paths.
  void MakePathsAbsolute(DictionaryValue* dict);

  // Sets the pref |key| for extension |id| to |value|.
  void UpdateExtensionPref(const std::string& id,
                           const std::wstring& key,
                           Value* value);

  // Deletes the pref dictionary for extension |id|.
  void DeleteExtensionPrefs(const std::string& id);

  // Reads a boolean pref from |ext| with key |pref_key|.
  // Return false if the value is false or kPrefBlacklist does not exist.
  bool ReadBooleanFromPref(DictionaryValue* ext, const std::wstring& pref_key);

  // Reads a boolean pref |pref_key| from extension with id |extension_id|.
  bool ReadExtensionPrefBoolean(const std::string& extension_id,
                                const std::wstring& pref_key);

  // Ensures and returns a mutable dictionary for extension |id|'s prefs.
  DictionaryValue* GetOrCreateExtensionPref(const std::string& id);

  // Same as above, but returns NULL if it doesn't exist.
  DictionaryValue* GetExtensionPref(const std::string& id);

  // Checks if kPrefBlacklist is set to true in the DictionaryValue.
  // Return false if the value is false or kPrefBlacklist does not exist.
  // This is used to decide if an extension is blacklisted.
  bool IsBlacklistBitSet(DictionaryValue* ext);

  // The pref service specific to this set of extension prefs.
  PrefService* prefs_;

  // Base extensions install directory.
  FilePath install_directory_;

  // The URLs of all of the toolstrips.
  URLList shelf_order_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefs);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H_

