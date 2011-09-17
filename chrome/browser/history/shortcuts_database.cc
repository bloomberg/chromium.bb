// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_database.h"

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_provider.h"
#include "chrome/common/guid.h"
#include "sql/statement.h"

using base::Time;

namespace {

// Using define instead of const char, so I could use ## in the statements.
#define kShortcutsDBName "omni_box_shortcuts"

// The maximum length allowed for form data.
const size_t kMaxDataLength = 2048;  // 2K is a hard limit on URLs URI.

string16 LimitDataSize(const string16& data) {
  if (data.size() > kMaxDataLength)
    return data.substr(0, kMaxDataLength);

  return data;
}

void BindShortcutToStatement(const shortcuts_provider::Shortcut& shortcut,
                             sql::Statement* s) {
  DCHECK(guid::IsValidGUID(shortcut.id));
  s->BindString(0, shortcut.id);
  s->BindString16(1, LimitDataSize(shortcut.text));
  s->BindString16(2, LimitDataSize(UTF8ToUTF16(shortcut.url.spec())));
  s->BindString16(3, LimitDataSize(shortcut.contents));
  s->BindString16(4, LimitDataSize(shortcut.contents_class_as_str()));
  s->BindString16(5, LimitDataSize(shortcut.description));
  s->BindString16(6, LimitDataSize(shortcut.description_class_as_str()));
  s->BindInt64(7, shortcut.last_access_time.ToInternalValue());
  s->BindInt(8, shortcut.number_of_hits);
}

shortcuts_provider::Shortcut ShortcutFromStatement(const sql::Statement& s) {
  return shortcuts_provider::Shortcut(s.ColumnString(0),
                                      s.ColumnString16(1),
                                      s.ColumnString16(2),
                                      s.ColumnString16(3),
                                      s.ColumnString16(4),
                                      s.ColumnString16(5),
                                      s.ColumnString16(6),
                                      s.ColumnInt64(7),
                                      s.ColumnInt(8));
}

bool DeleteShortcut(const char* field_name, const std::string& id,
                    sql::Connection& db) {
  sql::Statement s(db.GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s WHERE %s = ?", kShortcutsDBName,
                         field_name).c_str()));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, id);
  if (!s.Run())
    return false;
  return true;
}

}  // namespace

namespace history {

const FilePath::CharType ShortcutsDatabase::kShortcutsDatabaseName[] =
    FILE_PATH_LITERAL("Shortcuts");

ShortcutsDatabase::ShortcutsDatabase(const FilePath& folder_path)
    : database_path_(folder_path.Append(FilePath(kShortcutsDatabaseName))) {
}

ShortcutsDatabase::~ShortcutsDatabase() {}

bool ShortcutsDatabase::Init() {
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
    const shortcuts_provider::Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO " kShortcutsDBName
      " (id, text, url, contents, contents_class, description,"
      " description_class, last_access_time, number_of_hits) "
      "VALUES (?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED();
    return false;
  }
  BindShortcutToStatement(shortcut, &s);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool ShortcutsDatabase::UpdateShortcut(
    const shortcuts_provider::Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
    "UPDATE " kShortcutsDBName " "
      "SET id=?, text=?, url=?, contents=?, contents_class=?,"
      "     description=?, description_class=?, last_access_time=?,"
      "     number_of_hits=? "
      "WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

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
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DROP TABLE " kShortcutsDBName));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  if (!s.Run())
    return false;
  EnsureTable();
  db_.Execute("VACUUM");
  return true;
}

// Loads all of the shortcuts.
bool ShortcutsDatabase::LoadShortcuts(
    std::map<std::string, shortcuts_provider::Shortcut>* shortcuts) {
  DCHECK(shortcuts);
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, text, url, contents, contents_class, "
      "description, description_class, last_access_time, number_of_hits "
      "FROM " kShortcutsDBName));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  shortcuts->clear();
  while (s.Step()) {
    shortcuts->insert(std::make_pair(s.ColumnString(0),
                                     ShortcutFromStatement(s)));
  }
  return true;
}

bool ShortcutsDatabase::EnsureTable() {
  if (!db_.DoesTableExist(kShortcutsDBName)) {
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
                     "number_of_hits INTEGER)", kShortcutsDBName).c_str())) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

}  // namespace history
