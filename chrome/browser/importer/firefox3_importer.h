// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX3_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX3_IMPORTER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "googleurl/src/gurl.h"

struct sqlite3;

// Importer for Mozilla Firefox 3 and later.
// Firefox 3 stores its persistent information in a new system called places.
// http://wiki.mozilla.org/Places
class Firefox3Importer : public Importer {
 public:
  Firefox3Importer();

  // Importer methods.
  virtual void StartImport(const importer::ProfileInfo& profile_info,
                           uint16 items,
                           ImporterBridge* bridge);

 private:
  typedef std::map<int64, std::set<GURL> > FaviconMap;

  virtual ~Firefox3Importer();

  void ImportBookmarks();
  void ImportPasswords();
  void ImportHistory();
  void ImportSearchEngines();
  // Import the user's home page, unless it is set to default home page as
  // defined in browserconfig.properties.
  void ImportHomepage();
  void GetSearchEnginesXMLFiles(std::vector<FilePath>* files);

  // The struct stores the information about a bookmark item.
  struct BookmarkItem;
  typedef std::vector<BookmarkItem*> BookmarkList;

  // Gets the specific IDs of bookmark root node from |db|.
  void LoadRootNodeID(sqlite3* db, int* toolbar_folder_id,
                      int* menu_folder_id, int* unsorted_folder_id);

  // Loads all livemark IDs from database |db|.
  void LoadLivemarkIDs(sqlite3* db, std::set<int>* livemark);

  // Gets the bookmark folder with given ID, and adds the entry in |list|
  // if successful.
  void GetTopBookmarkFolder(sqlite3* db, int folder_id, BookmarkList* list);

  // Loads all children of the given folder, and appends them to the |list|.
  void GetWholeBookmarkFolder(sqlite3* db, BookmarkList* list,
                              size_t position, bool* empty_folder);

  // Loads the favicons given in the map from the database, loads the data,
  // and converts it into FaviconUsage structures.
  void LoadFavicons(sqlite3* db,
                    const FaviconMap& favicon_map,
                    std::vector<history::ImportedFavIconUsage>* favicons);

  FilePath source_path_;
  FilePath app_path_;

#if defined(OS_LINUX)
  // Stored because we can only access it from the UI thread. Not usable
  // in Mac because no access from out-of-process import.
  std::string locale_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Firefox3Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX3_IMPORTER_H_
