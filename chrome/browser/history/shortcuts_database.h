// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SHORTCUTS_DATABASE_H_
#define CHROME_BROWSER_HISTORY_SHORTCUTS_DATABASE_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "googleurl/src/gurl.h"
#include "sql/connection.h"

class Profile;

namespace history {

// This class manages the shortcut provider table within the SQLite database
// passed to the constructor. It expects the following schema:
//
// Note: The database stores time in seconds, UTC.
//
// omni_box_shortcuts
//   id                  Unique id of the entry (needed for the sync).
//   search_text         Text that shortcuts was searched with.
//   url                 The url of the shortcut.
//   contents            Contents of the original omni-box entry.
//   contents_matches    Comma separated matches of the |search_text| in
//                       |contents|, for example "0,0,5,3,9,0".
//   description         Description of the original omni-box entry.
//   description_matches Comma separated matches of the |search_text| in
//                       |description|.
//   last_access_time    Time the entry was accessed last, stored in seconds,
//                       UTC.
//   number_of_hits      Number of times that the entry has been selected.
class ShortcutsDatabase : public base::RefCountedThreadSafe<ShortcutsDatabase> {
 public:
  typedef std::map<std::string, ShortcutsBackend::Shortcut> GuidToShortcutMap;

  explicit ShortcutsDatabase(Profile* profile);

  bool Init();

  // Adds the ShortcutsProvider::Shortcut to the database.
  bool AddShortcut(const ShortcutsBackend::Shortcut& shortcut);

  // Updates timing and selection count for the ShortcutsProvider::Shortcut.
  bool UpdateShortcut(const ShortcutsBackend::Shortcut& shortcut);

  // Deletes the ShortcutsProvider::Shortcuts with the id.
  bool DeleteShortcutsWithIds(const std::vector<std::string>& shortcut_ids);

  // Deletes the ShortcutsProvider::Shortcuts with the url.
  bool DeleteShortcutsWithUrl(const std::string& shortcut_url_spec);

  // Deletes all of the ShortcutsProvider::Shortcuts.
  bool DeleteAllShortcuts();

  // Loads all of the shortcuts.
  bool LoadShortcuts(GuidToShortcutMap* shortcuts);

 private:
  friend class base::RefCountedThreadSafe<ShortcutsDatabase>;

  virtual ~ShortcutsDatabase();

  // Ensures that the table is present.
  bool EnsureTable();

  friend class ShortcutsDatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, AddShortcut);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, UpdateShortcut);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, DeleteShortcutsWithIds);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, DeleteShortcutsWithUrl);
  FRIEND_TEST_ALL_PREFIXES(ShortcutsDatabaseTest, LoadShortcuts);

  // The sql database. Not valid until Init is called.
  sql::Connection db_;
  base::FilePath database_path_;

  static const base::FilePath::CharType kShortcutsDatabaseName[];

  DISALLOW_COPY_AND_ASSIGN(ShortcutsDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_SHORTCUTS_DATABASE_H_
