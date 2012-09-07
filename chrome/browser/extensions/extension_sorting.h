// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SORTING_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SORTING_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/extensions/extension.h"
#include "sync/api/string_ordinal.h"

class ExtensionScopedPrefs;
class ExtensionServiceInterface;
class PrefService;

class ExtensionSorting {
 public:
  ExtensionSorting(ExtensionScopedPrefs* extension_scoped_prefs,
                   PrefService* pref_service);
  ~ExtensionSorting();

  // Set up the ExtensionService to inform of changes that require syncing.
  void SetExtensionService(ExtensionServiceInterface* extension_service);

  // Properly initialize ExtensionSorting internal values that require
  // |extension_ids|.
  void Initialize(
      const extensions::ExtensionPrefs::ExtensionIds& extension_ids);

  // Resolves any conflicts the might be created as a result of syncing that
  // results in two icons having the same page and app launch ordinal. After
  // this is called it is guaranteed that there are no collisions of NTP icons.
  void FixNTPOrdinalCollisions();

  // This ensures that the extension has valid ordinals, and if it doesn't then
  // properly initialize them.
  void EnsureValidOrdinals(const std::string& extension_id);

  // Updates the app launcher value for the moved extension so that it is now
  // located after the given predecessor and before the successor.
  // Empty strings are used to indicate no successor or predecessor.
  void OnExtensionMoved(const std::string& moved_extension_id,
                        const std::string& predecessor_extension_id,
                        const std::string& successor_extension_id);

  // Get the application launch ordinal for an app with |extension_id|. This
  // determines the order in which the app appears on the page it's on in the
  // New Tab Page (Note that you can compare app launch ordinals only if the
  // apps are on the same page). A string value close to |a*| generally
  // indicates top left. If the extension has no launch ordinal, an invalid
  // StringOrdinal is returned.
  syncer::StringOrdinal GetAppLaunchOrdinal(
      const std::string& extension_id) const;

  // Sets a specific launch ordinal for an app with |extension_id|.
  void SetAppLaunchOrdinal(const std::string& extension_id,
                           const syncer::StringOrdinal& new_app_launch_ordinal);

  // Returns a StringOrdinal that is lower than any app launch ordinal for the
  // given page.
  syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const;

  // Returns a StringOrdinal that is higher than any app launch ordinal for the
  // given page.
  syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const;

  // Returns a StringOrdinal that is lower than any existing page ordinal.
  syncer::StringOrdinal CreateFirstAppPageOrdinal() const;

  // Gets the page a new app should install to, which is the earliest non-full
  // page.  The returned ordinal may correspond to a page that doesn't yet exist
  // if all pages are full.
  syncer::StringOrdinal GetNaturalAppPageOrdinal() const;

  // Get the page ordinal for an app with |extension_id|. This determines
  // which page an app will appear on in page-based NTPs.  If the app has no
  // page specified, an invalid StringOrdinal is returned.
  syncer::StringOrdinal GetPageOrdinal(const std::string& extension_id) const;

  // Sets a specific page ordinal for an app with |extension_id|.
  void SetPageOrdinal(const std::string& extension_id,
                      const syncer::StringOrdinal& new_page_ordinal);

  // Removes the ordinal values for an app.
  void ClearOrdinals(const std::string& extension_id);

  // Convert the page StringOrdinal value to its integer equivalent. This takes
  // O(# of apps) worst-case.
  int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const;

  // Converts the page index integer to its StringOrdinal equivalent. This takes
  // O(# of apps) worst-case.
  syncer::StringOrdinal PageIntegerAsStringOrdinal(size_t page_index);

 private:
  // Unit tests.
  friend class ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage;
  friend class ExtensionSortingInitializeWithNoApps;
  friend class ExtensionSortingPageOrdinalMapping;

  // An enum used by GetMinOrMaxAppLaunchOrdinalsOnPage to specify which
  // value should be returned.
  enum AppLaunchOrdinalReturn {MIN_ORDINAL, MAX_ORDINAL};

  // This function returns the lowest ordinal on |page_ordinal| if
  // |return_value| == AppLaunchOrdinalReturn::MIN_ORDINAL, otherwise it returns
  // the largest ordinal on |page_ordinal|. If there are no apps on the page
  // then an invalid StringOrdinal is returned. It is an error to call this
  // function with an invalid |page_ordinal|.
  syncer::StringOrdinal GetMinOrMaxAppLaunchOrdinalsOnPage(
      const syncer::StringOrdinal& page_ordinal,
      AppLaunchOrdinalReturn return_type) const;

  // Initialize the |page_ordinal_map_| with the page ordinals used by the
  // given extensions.
  void InitializePageOrdinalMap(
      const extensions::ExtensionPrefs::ExtensionIds& extension_ids);

  // Migrates the app launcher and page index values.
  void MigrateAppIndex(
      const extensions::ExtensionPrefs::ExtensionIds& extension_ids);

  // Called to add a new mapping value for |extension_id| with a page ordinal
  // of |page_ordinal| and a app launch ordinal of |app_launch_ordinal|. This
  // works with valid and invalid StringOrdinals.
  void AddOrdinalMapping(const std::string& extension_id,
                         const syncer::StringOrdinal& page_ordinal,
                         const syncer::StringOrdinal& app_launch_ordinal);

  // Ensures |ntp_ordinal_map_| is of |minimum_size| number of entries.
  void CreateOrdinalsIfNecessary(size_t minimum_size);

  // Removes the mapping for |extension_id| with a page ordinal of
  // |page_ordinal| and a app launch ordinal of |app_launch_ordinal|. If there
  // is not matching map, nothing happens. This works with valid and invalid
  // StringOrdinals.
  void RemoveOrdinalMapping(const std::string& extension_id,
                            const syncer::StringOrdinal& page_ordinal,
                            const syncer::StringOrdinal& app_launch_ordinal);

  // Syncs the extension if needed. It is an error to call this if the
  // extension is not an application.
  void SyncIfNeeded(const std::string& extension_id);

  ExtensionScopedPrefs* extension_scoped_prefs_;  // Weak, owns this instance.
  PrefService* pref_service_;  // Weak.
  ExtensionServiceInterface* extension_service_;  // Weak.

  // A map of all the StringOrdinal page ordinals mapping to the collections of
  // app launch ordinals that exist on that page. This is used for mapping
  // StringOrdinals to their Integer equivalent as well as quick lookup of the
  // any collision of on the NTP (icons with the same page and same app launch
  // ordinals). The possiblity of collisions means that a multimap must be used
  // (although the collisions must all be resolved once all the syncing is
  // done).
  // The StringOrdinal is the app launch ordinal and the string is the extension
  // id.
  typedef std::multimap<
      syncer::StringOrdinal, std::string,
    syncer::StringOrdinal::LessThanFn> AppLaunchOrdinalMap;
  // The StringOrdinal is the page ordinal and the AppLaunchOrdinalMap is the
  // contents of that page.
  typedef std::map<
      syncer::StringOrdinal, AppLaunchOrdinalMap,
    syncer::StringOrdinal::LessThanFn> PageOrdinalMap;
  PageOrdinalMap ntp_ordinal_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSorting);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SORTING_H_
