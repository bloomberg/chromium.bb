// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SORTING_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SORTING_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/string_ordinal.h"

class ExtensionScopedPrefs;

class ExtensionSorting {
 public:
  explicit ExtensionSorting(ExtensionScopedPrefs* extension_scoped_prefs,
                            PrefService* pref_service);
  ~ExtensionSorting();

  // Migrates the app launcher and page index values.
  void MigrateAppIndex(const ExtensionPrefs::ExtensionIdSet& extension_ids);

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

 private:
  friend class ExtensionSortingGetMinOrMaxAppLaunchOrdinalsOnPage;
    // Unit test.

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
  void InitializePageOrdinalMap(
      const ExtensionPrefs::ExtensionIdSet& extension_ids);

  // Called when an application changes the value of its page ordinal so that
  // |page_ordinal_map_| is aware that |old_value| page ordinal has been
  // replace by the |new_value| page ordinal and adjusts its mapping
  // accordingly. This works with valid and invalid StringOrdinals.
  void UpdatePageOrdinalMap(const StringOrdinal& old_value,
                            const StringOrdinal& new_value);

  ExtensionScopedPrefs* extension_scoped_prefs_;  // Weak, owns this instance.
  PrefService* pref_service_;  // Weak.

  // A map of all the StringOrdinal page indices mapping to how often they are
  // used, this is used for mapping the StringOrdinals to their integer
  // equivalent as well as quick lookup of the sorted StringOrdinals.
  // TODO(csharp) Convert this to a two-layer map to allow page ordinal lookup
  // by id and vice versa.
  typedef std::map<StringOrdinal, int, StringOrdinalLessThan> PageOrdinalMap;
  PageOrdinalMap page_ordinal_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSorting);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SORTING_H_
