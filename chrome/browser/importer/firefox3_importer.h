// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX3_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX3_IMPORTER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "chrome/browser/importer/importer.h"

class GURL;

namespace history {
struct ImportedFaviconUsage;
}

namespace sql {
class Connection;
}

// Importer for Mozilla Firefox 3 and later.
// Firefox 3 stores its persistent information in a new system called places.
// http://wiki.mozilla.org/Places
class Firefox3Importer : public Importer {
 public:
  Firefox3Importer();

  // Importer:
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;

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
  void GetSearchEnginesXMLFiles(std::vector<base::FilePath>* files);

  // The struct stores the information about a bookmark item.
  struct BookmarkItem;
  typedef std::vector<BookmarkItem*> BookmarkList;

  // Gets the specific IDs of bookmark root node from |db|.
  void LoadRootNodeID(sql::Connection* db, int* toolbar_folder_id,
                      int* menu_folder_id, int* unsorted_folder_id);

  // Loads all livemark IDs from database |db|.
  void LoadLivemarkIDs(sql::Connection* db, std::set<int>* livemark);

  // Gets the bookmark folder with given ID, and adds the entry in |list|
  // if successful.
  void GetTopBookmarkFolder(sql::Connection* db,
                            int folder_id,
                            BookmarkList* list);

  // Loads all children of the given folder, and appends them to the |list|.
  void GetWholeBookmarkFolder(sql::Connection* db, BookmarkList* list,
                              size_t position, bool* empty_folder);

  // Loads the favicons given in the map from the database, loads the data,
  // and converts it into FaviconUsage structures.
  void LoadFavicons(sql::Connection* db,
                    const FaviconMap& favicon_map,
                    std::vector<history::ImportedFaviconUsage>* favicons);

  base::FilePath source_path_;
  base::FilePath app_path_;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Stored because we can only access it from the UI thread. Not usable
  // in Mac because no access from out-of-process import.
  std::string locale_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Firefox3Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX3_IMPORTER_H_
