// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor_database.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/guid.h"
#include "content/public/browser/browser_thread.h"
#include "sql/statement.h"

namespace {

const char kAutocompleteActionPredictorTableName[] = "network_action_predictor";
const FilePath::CharType kAutocompleteActionPredictorDatabaseName[] =
    FILE_PATH_LITERAL("Network Action Predictor");

// The maximum length allowed for strings in the database.
const size_t kMaxDataLength = 2048;

void BindRowToStatement(const AutocompleteActionPredictorDatabase::Row& row,
                        sql::Statement* statement) {
  DCHECK(guid::IsValidGUID(row.id));
  statement->BindString(0, row.id);
  statement->BindString16(1, row.user_text.substr(0, kMaxDataLength));
  statement->BindString(2, row.url.spec().substr(0, kMaxDataLength));
  statement->BindInt(3, row.number_of_hits);
  statement->BindInt(4, row.number_of_misses);
}

bool StepAndInitializeRow(sql::Statement* statement,
                          AutocompleteActionPredictorDatabase::Row* row) {
  if (!statement->Step())
    return false;

  row->id = statement->ColumnString(0);
  row->user_text = statement->ColumnString16(1);
  row->url = GURL(statement->ColumnString(2));
  row->number_of_hits = statement->ColumnInt(3);
  row->number_of_misses = statement->ColumnInt(4);
  return true;
}

void LogDatabaseStats(const FilePath& db_path, sql::Connection* db) {
  int64 db_size;
  bool success = file_util::GetFileSize(db_path, &db_size);
  DCHECK(success) << "Failed to get file size for " << db_path.value();
  UMA_HISTOGRAM_MEMORY_KB("NetworkActionPredictor.DatabaseSizeKB",
                          static_cast<int>(db_size / 1024));

  sql::Statement count_statement(db->GetUniqueStatement(
      base::StringPrintf("SELECT count(id) FROM %s",
                         kAutocompleteActionPredictorTableName).c_str()));
  if (!count_statement.Step())
    return;
  UMA_HISTOGRAM_COUNTS("NetworkActionPredictor.DatabaseRowCount",
                       count_statement.ColumnInt(0));
}

}

AutocompleteActionPredictorDatabase::Row::Row()
    : number_of_hits(0),
      number_of_misses(0) {
}

AutocompleteActionPredictorDatabase::Row::Row(const Row::Id& id,
                                         const string16& user_text,
                                         const GURL& url,
                                         int number_of_hits,
                                         int number_of_misses)
    : id(id),
      user_text(user_text),
      url(url),
      number_of_hits(number_of_hits),
      number_of_misses(number_of_misses) {
}

AutocompleteActionPredictorDatabase::AutocompleteActionPredictorDatabase(
    Profile* profile)
    : db_path_(profile->GetPath().Append(
        kAutocompleteActionPredictorDatabaseName)) {
}

AutocompleteActionPredictorDatabase::~AutocompleteActionPredictorDatabase() {
}

void AutocompleteActionPredictorDatabase::Initialize() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  db_.set_exclusive_locking();
  if (!db_.Open(db_path_)) {
    canceled_.Set();
    return;
  }

  if (!db_.DoesTableExist(kAutocompleteActionPredictorTableName))
    CreateTable();

  LogDatabaseStats(db_path_, &db_);
}

void AutocompleteActionPredictorDatabase::GetRow(const Row::Id& id, Row* row) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT * FROM %s WHERE id=?",
          kAutocompleteActionPredictorTableName).c_str()));
  statement.BindString(0, id);

  bool success = StepAndInitializeRow(&statement, row);
  DCHECK(success) << "Failed to get row " << id << " from "
                  << kAutocompleteActionPredictorTableName;
}

void AutocompleteActionPredictorDatabase::GetAllRows(
    std::vector<AutocompleteActionPredictorDatabase::Row>* row_buffer) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));
  CHECK(row_buffer);
  row_buffer->clear();

  if (canceled_.IsSet())
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT * FROM %s", kAutocompleteActionPredictorTableName).c_str()));

  Row row;
  while (StepAndInitializeRow(&statement, &row))
    row_buffer->push_back(row);
}

void AutocompleteActionPredictorDatabase::AddRow(
    const AutocompleteActionPredictorDatabase::Row& row) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s "
          "(id, user_text, url, number_of_hits, number_of_misses) "
          "VALUES (?,?,?,?,?)",
          kAutocompleteActionPredictorTableName).c_str()));
  BindRowToStatement(row, &statement);

  bool success = statement.Run();
  DCHECK(success) << "Failed to insert row " << row.id << " into "
                  << kAutocompleteActionPredictorTableName;
}

void AutocompleteActionPredictorDatabase::UpdateRow(
    const AutocompleteActionPredictorDatabase::Row& row) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf(
          "UPDATE %s "
          "SET id=?, user_text=?, url=?, number_of_hits=?, number_of_misses=? "
          "WHERE id=?1", kAutocompleteActionPredictorTableName).c_str()));
  BindRowToStatement(row, &statement);

  statement.Run();
  DCHECK_GT(db_.GetLastChangeCount(), 0);
}

void AutocompleteActionPredictorDatabase::DeleteRow(const Row::Id& id) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  DeleteRows(std::vector<Row::Id>(1, id));
}

void AutocompleteActionPredictorDatabase::DeleteRows(
    const std::vector<Row::Id>& id_list) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  sql::Statement statement(db_.GetUniqueStatement(base::StringPrintf(
          "DELETE FROM %s WHERE id=?",
          kAutocompleteActionPredictorTableName).c_str()));

  db_.BeginTransaction();
  for (std::vector<Row::Id>::const_iterator it = id_list.begin();
       it != id_list.end(); ++it) {
    statement.BindString(0, *it);
    statement.Run();
    statement.Reset(true);
  }
  db_.CommitTransaction();
}

void AutocompleteActionPredictorDatabase::DeleteAllRows() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s",
                         kAutocompleteActionPredictorTableName).c_str()));

  statement.Run();
}

void AutocompleteActionPredictorDatabase::BeginTransaction() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  db_.BeginTransaction();
}

void AutocompleteActionPredictorDatabase::CommitTransaction() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::DB));

  if (canceled_.IsSet())
    return;

  db_.CommitTransaction();
}

void AutocompleteActionPredictorDatabase::OnPredictorDestroyed() {
  canceled_.Set();
}

void AutocompleteActionPredictorDatabase::CreateTable() {
  bool success = db_.Execute(base::StringPrintf(
      "CREATE TABLE %s ( "
      "id TEXT PRIMARY KEY, "
      "user_text TEXT, "
      "url TEXT, "
      "number_of_hits INTEGER, "
      "number_of_misses INTEGER)",
      kAutocompleteActionPredictorTableName).c_str());
  DCHECK(success) << "Failed to create "
                  << kAutocompleteActionPredictorTableName << " table.";
}
