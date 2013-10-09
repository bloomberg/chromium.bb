// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_database.h"

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

void BindShortcutToStatement(
    const history::ShortcutsBackend::Shortcut& shortcut,
    sql::Statement* s) {
  DCHECK(base::IsValidGUID(shortcut.id));
  s->BindString(0, shortcut.id);
  s->BindString16(1, shortcut.text);
  s->BindString16(2, shortcut.match_core.fill_into_edit);
  s->BindString(3, shortcut.match_core.destination_url.spec());
  s->BindString16(4, shortcut.match_core.contents);
  s->BindString(5, AutocompleteMatch::ClassificationsToString(
      shortcut.match_core.contents_class));
  s->BindString16(6, shortcut.match_core.description);
  s->BindString(7, AutocompleteMatch::ClassificationsToString(
      shortcut.match_core.description_class));
  s->BindInt(8, shortcut.match_core.transition);
  s->BindInt(9, shortcut.match_core.type);
  s->BindString16(10, shortcut.match_core.keyword);
  s->BindInt64(11, shortcut.last_access_time.ToInternalValue());
  s->BindInt(12, shortcut.number_of_hits);
}

bool DeleteShortcut(const char* field_name,
                    const std::string& id,
                    sql::Connection& db) {
  sql::Statement s(db.GetUniqueStatement(
      base::StringPrintf("DELETE FROM omni_box_shortcuts WHERE %s = ?",
                         field_name).c_str()));
  s.BindString(0, id);
  return s.Run();
}

}  // namespace

namespace history {

ShortcutsDatabase::ShortcutsDatabase(const base::FilePath& database_path)
    : database_path_(database_path) {
}

bool ShortcutsDatabase::Init() {
  db_.set_histogram_tag("Shortcuts");

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  // Attach the database to our index file.
  return db_.Open(database_path_) && EnsureTable();
}

bool ShortcutsDatabase::AddShortcut(
    const ShortcutsBackend::Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO omni_box_shortcuts (id, text, fill_into_edit, url, "
          "contents, contents_class, description, description_class, "
          "transition, type, keyword, last_access_time, number_of_hits) "
          "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  BindShortcutToStatement(shortcut, &s);
  return s.Run();
}

bool ShortcutsDatabase::UpdateShortcut(
    const ShortcutsBackend::Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE omni_box_shortcuts SET id=?, text=?, fill_into_edit=?, url=?, "
          "contents=?, contents_class=?, description=?, description_class=?, "
          "transition=?, type=?, keyword=?, last_access_time=?, "
          "number_of_hits=? WHERE id=?"));
  BindShortcutToStatement(shortcut, &s);
  s.BindString(13, shortcut.id);
  return s.Run();
}

bool ShortcutsDatabase::DeleteShortcutsWithIds(
    const std::vector<std::string>& shortcut_ids) {
  bool success = true;
  db_.BeginTransaction();
  for (std::vector<std::string>::const_iterator it(shortcut_ids.begin());
       it != shortcut_ids.end(); ++it) {
    success &= DeleteShortcut("id", *it, db_);
  }
  db_.CommitTransaction();
  return success;
}

bool ShortcutsDatabase::DeleteShortcutsWithUrl(
    const std::string& shortcut_url_spec) {
  return DeleteShortcut("url", shortcut_url_spec, db_);
}

bool ShortcutsDatabase::DeleteAllShortcuts() {
  if (!db_.Execute("DELETE FROM omni_box_shortcuts"))
    return false;

  ignore_result(db_.Execute("VACUUM"));
  return true;
}

void ShortcutsDatabase::LoadShortcuts(GuidToShortcutMap* shortcuts) {
  DCHECK(shortcuts);
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, text, fill_into_edit, url, contents, contents_class, "
          "description, description_class, transition, type, keyword, "
          "last_access_time, number_of_hits FROM omni_box_shortcuts"));

  shortcuts->clear();
  while (s.Step()) {
    shortcuts->insert(std::make_pair(
        s.ColumnString(0),
        ShortcutsBackend::Shortcut(
            s.ColumnString(0),            // id
            s.ColumnString16(1),          // text
            ShortcutsBackend::Shortcut::MatchCore(
                s.ColumnString16(2),      // fill_into_edit
                GURL(s.ColumnString(3)),  // destination_url
                s.ColumnString16(4),      // contents
                AutocompleteMatch::ClassificationsFromString(s.ColumnString(5)),
                                          // contents_class
                s.ColumnString16(6),      // description
                AutocompleteMatch::ClassificationsFromString(s.ColumnString(7)),
                                          // description_class
                static_cast<content::PageTransition>(s.ColumnInt(8)),
                                          // transition
                static_cast<AutocompleteMatch::Type>(s.ColumnInt(9)),
                                          // type
                s.ColumnString16(10)),    // keyword
            base::Time::FromInternalValue(s.ColumnInt64(11)),
                                          // last_access_time
            s.ColumnInt(12))));           // number_of_hits
  }
}

ShortcutsDatabase::~ShortcutsDatabase() {
}

bool ShortcutsDatabase::EnsureTable() {
  if (!db_.DoesTableExist("omni_box_shortcuts")) {
    return db_.Execute(
        "CREATE TABLE omni_box_shortcuts (id VARCHAR PRIMARY KEY, "
            "text VARCHAR, fill_into_edit VARCHAR, url VARCHAR, "
            "contents VARCHAR, contents_class VARCHAR, description VARCHAR, "
            "description_class VARCHAR, transition INTEGER, type INTEGER, "
            "keyword VARCHAR, last_access_time INTEGER, "
            "number_of_hits INTEGER)");
  }

  // The first version of the shortcuts table lacked the fill_into_edit,
  // transition, type, and keyword columns.
  if (!db_.DoesColumnExist("omni_box_shortcuts", "fill_into_edit")) {
    // Perform the upgrade in a transaction to ensure it doesn't happen
    // incompletely.
    sql::Transaction transaction(&db_);
    transaction.Begin();
    return
        db_.Execute("ALTER TABLE omni_box_shortcuts "
            "ADD COLUMN fill_into_edit VARCHAR") &&
        db_.Execute("UPDATE omni_box_shortcuts SET fill_into_edit = url") &&
        db_.Execute("ALTER TABLE omni_box_shortcuts "
            "ADD COLUMN transition INTEGER") &&
        db_.Execute(base::StringPrintf(
            "UPDATE omni_box_shortcuts SET transition = %d",
            static_cast<int>(content::PAGE_TRANSITION_TYPED)).c_str()) &&
        db_.Execute("ALTER TABLE omni_box_shortcuts ADD COLUMN type INTEGER") &&
        db_.Execute(base::StringPrintf(
            "UPDATE omni_box_shortcuts SET type = %d",
            static_cast<int>(AutocompleteMatchType::HISTORY_TITLE)).c_str()) &&
        db_.Execute("ALTER TABLE omni_box_shortcuts "
            "ADD COLUMN keyword VARCHAR") &&
        transaction.Commit();
  }

  return true;
}

}  // namespace history
