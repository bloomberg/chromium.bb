// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_SAFARI_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_SAFARI_IMPORTER_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"

#if __OBJC__
@class NSDictionary;
@class NSString;
#else
class NSDictionary;
class NSString;
#endif

// Importer for Safari on OS X.
class SafariImporter : public Importer {
 public:
  // |library_dir| is the full path to the ~/Library directory,
  // We pass it in as a parameter for testing purposes.
  explicit SafariImporter(const FilePath& library_dir);

  // Importer:
  virtual void StartImport(const importer::ProfileInfo& profile_info,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;


 // Does this user account have a Safari Profile and if so, what items
 // are supported?
 // in: library_dir - ~/Library or a standin for testing purposes.
 // out: services_supported - the service supported for import.
 // returns true if we can import the Safari profile.
 static bool CanImport(const FilePath& library_dir, uint16 *services_supported);
 private:
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, BookmarkImport);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, FavIconImport);
  FRIEND_TEST_ALL_PREFIXES(SafariImporterTest, HistoryImport);

  virtual ~SafariImporter();

  // Multiple URLs can share the same FavIcon, this is a map
  // of URLs -> IconIDs that we load as a temporary step before
  // actually loading the icons.
  typedef std::map<int64, std::set<GURL> > FaviconMap;

  void ImportBookmarks();
  void ImportPasswords();
  void ImportHistory();

  // Parse Safari's stored bookmarks.
  void ParseBookmarks(std::vector<ProfileWriter::BookmarkEntry>* bookmarks);

  // Function to recursively read Bookmarks out of Safari plist.
  // |bookmark_folder| The dictionary containing a folder to parse.
  // |parent_path_elements| Path elements up to this point.
  // |is_in_toolbar| Is this folder in the toolbar.
  // |out_bookmarks| BookMark element array to write into.
  void RecursiveReadBookmarksFolder(
      NSDictionary* bookmark_folder,
      const std::vector<std::wstring>& parent_path_elements,
      bool is_in_toolbar,
      std::vector<ProfileWriter::BookmarkEntry>* out_bookmarks);


  // Converts history time stored by Safari as a double serialized as a string,
  // to seconds-since-UNIX-Ephoch-format used by Chrome.
  double HistoryTimeToEpochTime(NSString* history_time);

  // Parses Safari's history and loads it into the input array.
  void ParseHistoryItems(std::vector<history::URLRow>* history_items);

  // Loads the favicon Database file, returns NULL on failure.
  sqlite3* OpenFavIconDB();

  // Loads the urls associated with the favicons into favicon_map;
  void ImportFavIconURLs(sqlite3* db, FaviconMap* favicon_map);

  // Loads and reencodes the individual favicons.
  void LoadFaviconData(sqlite3* db,
                       const FaviconMap& favicon_map,
                       std::vector<history::ImportedFavIconUsage>* favicons);

  FilePath library_dir_;

  DISALLOW_COPY_AND_ASSIGN(SafariImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_SAFARI_IMPORTER_H_
