// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_database.h"

#include <map>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "sql/statement.h"

namespace {

const char* kShortcutsTableName = "omni_box_shortcuts";

void BindShortcutToStatement(
    const history::ShortcutsBackend::Shortcut& shortcut,
    sql::Statement* s) {
  DCHECK(base::IsValidGUID(shortcut.id));
  s->BindString(0, shortcut.id);
  s->BindString16(1, shortcut.text);
  // fill_into_edit
  s->BindString(2, shortcut.match_core.destination_url.spec());
  s->BindString16(3, shortcut.match_core.contents);
  s->BindString(4, AutocompleteMatch::ClassificationsToString(
      shortcut.match_core.contents_class));
  s->BindString16(5, shortcut.match_core.description);
  s->BindString(6, AutocompleteMatch::ClassificationsToString(
      shortcut.match_core.description_class));
  // transition
  // type
  // keyword
  s->BindInt64(7, shortcut.last_access_time.ToInternalValue());
  s->BindInt(8, shortcut.number_of_hits);
}

bool DeleteShortcut(const char* field_name,
                    const std::string& id,
                    sql::Connection& db) {
  sql::Statement s(db.GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s WHERE %s = ?", kShortcutsTableName,
                         field_name).c_str()));
  s.BindString(0, id);

  return s.Run();
}

}  // namespace

namespace history {

ShortcutsDatabase::ShortcutsDatabase(Profile* profile) {
  database_path_ = profile->GetPath().Append(chrome::kShortcutsDatabaseName);
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
  if (!db_.Open(database_path_))
    return false;

  if (!EnsureTable())
    return false;
  return true;
}

bool ShortcutsDatabase::AddShortcut(
    const ShortcutsBackend::Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("INSERT INTO %s (id, text, url, contents, "
          "contents_class, description, description_class, last_access_time, "
          "number_of_hits) VALUES (?,?,?,?,?,?,?,?,?)",
          kShortcutsTableName).c_str()));
  BindShortcutToStatement(shortcut, &s);

  return s.Run();
}

bool ShortcutsDatabase::UpdateShortcut(
    const ShortcutsBackend::Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("UPDATE %s SET id=?, text=?, url=?, contents=?, "
          "contents_class=?, description=?, description_class=?, "
          "last_access_time=?, number_of_hits=? WHERE id=?",
          kShortcutsTableName).c_str()));
  BindShortcutToStatement(shortcut, &s);
  s.BindString(9, shortcut.id);

  bool result = s.Run();
  DCHECK_GT(db_.GetLastChangeCount(), 0);
  return result;
}

bool ShortcutsDatabase::DeleteShortcutsWithIds(
    const std::vector<std::string>& shortcut_ids) {
  bool success = true;
  db_.BeginTransaction();
  for (std::vector<std::string>::const_iterator it = shortcut_ids.begin();
       it != shortcut_ids.end(); ++it) {
    if (!DeleteShortcut("id", *it, db_))
      success = false;
  }
  db_.CommitTransaction();
  return success;
}

bool ShortcutsDatabase::DeleteShortcutsWithUrl(
    const std::string& shortcut_url_spec) {
  return DeleteShortcut("url", shortcut_url_spec, db_);
}

bool ShortcutsDatabase::DeleteAllShortcuts() {
  if (!db_.Execute(base::StringPrintf("DELETE FROM %s",
                                      kShortcutsTableName).c_str()))
    return false;

  ignore_result(db_.Execute("VACUUM"));
  return true;
}

// Loads all of the shortcuts.
bool ShortcutsDatabase::LoadShortcuts(GuidToShortcutMap* shortcuts) {
  DCHECK(shortcuts);
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("SELECT id, text, url, contents, contents_class, "
          "description, description_class, last_access_time, number_of_hits "
          "FROM %s", kShortcutsTableName).c_str()));

  if (!s.is_valid())
    return false;

  shortcuts->clear();
  while (s.Step()) {
    shortcuts->insert(std::make_pair(
        s.ColumnString(0),
        ShortcutsBackend::Shortcut(
            s.ColumnString(0), s.ColumnString16(1),
            ShortcutsBackend::Shortcut::MatchCore(
                /*fill_into_edit*/string16(), GURL(s.ColumnString(2)),
                s.ColumnString16(3),
                AutocompleteMatch::ClassificationsFromString(s.ColumnString(4)),
                s.ColumnString16(5),
                AutocompleteMatch::ClassificationsFromString(s.ColumnString(6)),
                /*transition*/content::PAGE_TRANSITION_TYPED,
                /*type*/AutocompleteMatchType::HISTORY_TITLE,
                /*keyword*/string16()),
            base::Time::FromInternalValue(s.ColumnInt64(7)), s.ColumnInt(8))));
  }
  return true;
}

ShortcutsDatabase::~ShortcutsDatabase() {}

bool ShortcutsDatabase::EnsureTable() {
  if (!db_.DoesTableExist(kShortcutsTableName)) {
    if (!db_.Execute(base::StringPrintf(
                     "CREATE TABLE %s ( "
                     "id VARCHAR PRIMARY KEY, "
                     "text VARCHAR, "
                     "url VARCHAR, "
                     "contents VARCHAR, "
                     "contents_class VARCHAR, "
                     "description VARCHAR, "
                     "description_class VARCHAR, "
                     "last_access_time INTEGER, "
                     "number_of_hits INTEGER)", kShortcutsTableName).c_str())) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

}  // namespace history
