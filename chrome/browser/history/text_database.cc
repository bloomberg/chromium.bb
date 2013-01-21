// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <set>
#include <string>

#include "chrome/browser/history/text_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "sql/statement.h"
#include "sql/transaction.h"

// There are two tables in each database, one full-text search (FTS) table which
// indexes the contents and title of the pages. The other is a regular SQLITE
// table which contains non-indexed information about the page. All columns of
// a FTS table are indexed using the text search algorithm, which isn't what we
// want for things like times. If this were in the FTS table, there would be
// different words in the index for each time number.
//
// "pages" FTS table:
//   url    URL of the page so searches will match the URL.
//   title  Title of the page.
//   body   Body of the page.
//
// "info" regular table:
//   time     Time the corresponding FTS entry was visited.
//
// We do joins across these two tables by using their internal rowids, which we
// keep in sync between the two tables. The internal rowid is the only part of
// an FTS table that is indexed like a normal table, and the index over it is
// free since sqlite always indexes the internal rowid.

namespace history {

namespace {

// Version 1 uses FTS2 for index files.
// Version 2 uses FTS3.
static const int kCurrentVersionNumber = 2;
static const int kCompatibleVersionNumber = 2;

// Snippet computation relies on the index of the columns in the original
// create statement. These are the 0-based indices (as strings) of the
// corresponding columns.
const char kTitleColumnIndex[] = "1";
const char kBodyColumnIndex[] = "2";

// The string prepended to the database identifier to generate the filename.
const FilePath::CharType kFilePrefix[] = FILE_PATH_LITERAL("History Index ");

}  // namespace

TextDatabase::Match::Match() {}

TextDatabase::Match::~Match() {}

TextDatabase::TextDatabase(const FilePath& path,
                           DBIdent id,
                           bool allow_create)
    : path_(path),
      ident_(id),
      allow_create_(allow_create) {
  // Compute the file name.
  file_name_ = path_.Append(IDToFileName(ident_));
}

TextDatabase::~TextDatabase() {
}

// static
const FilePath::CharType* TextDatabase::file_base() {
  return kFilePrefix;
}

// static
FilePath TextDatabase::IDToFileName(DBIdent id) {
  // Identifiers are intended to be a combination of the year and month, for
  // example, 200801 for January 2008. We convert this to
  // "History Index 2008-01". However, we don't make assumptions about this
  // scheme: the caller should assign IDs as it feels fit with the knowledge
  // that they will apppear on disk in this form.
  FilePath::StringType filename(file_base());
  base::StringAppendF(&filename, FILE_PATH_LITERAL("%d-%02d"),
                      id / 100, id % 100);
  return FilePath(filename);
}

// static
TextDatabase::DBIdent TextDatabase::FileNameToID(const FilePath& file_path) {
  FilePath::StringType file_name = file_path.BaseName().value();

  // We don't actually check the prefix here. Since the file system could
  // be case insensitive in ways we can't predict (NTFS), checking could
  // potentially be the wrong thing to do. Instead, we just look for a suffix.
  static const size_t kIDStringLength = 7;  // Room for "xxxx-xx".
  if (file_name.length() < kIDStringLength)
    return 0;
  const FilePath::StringType suffix(
      &file_name[file_name.length() - kIDStringLength]);

  if (suffix.length() != kIDStringLength ||
      suffix[4] != FILE_PATH_LITERAL('-')) {
    return 0;
  }

  // TODO: Once StringPiece supports a templated interface over the
  // underlying string type, use it here instead of substr, since that
  // will avoid needless string copies.  StringPiece cannot be used
  // right now because FilePath::StringType could use either 8 or 16 bit
  // characters, depending on the OS.
  int year, month;
  base::StringToInt(suffix.substr(0, 4), &year);
  base::StringToInt(suffix.substr(5, 2), &month);

  return year * 100 + month;
}

bool TextDatabase::Init() {
  // Make sure, if we're not allowed to create the file, that it exists.
  if (!allow_create_) {
    if (!file_util::PathExists(file_name_))
      return false;
  }

  db_.set_error_histogram_name("Sqlite.Text.Error");

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // The default cache size is 2000 which give >8MB of data. Since we will often
  // have 2-3 of these objects, each with their own 8MB, this adds up very fast.
  // We therefore reduce the size so when there are multiple objects, we're not
  // too big.
  db_.set_cache_size(512);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  // Attach the database to our index file.
  if (!db_.Open(file_name_))
    return false;

  // Meta table tracking version information.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return false;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // This version is too new. We don't bother notifying the user on this
    // error, and just fail to use the file. Normally if they have version skew,
    // they will get it for the main history file and it won't be necessary
    // here. If that's not the case, since this is only indexed data, it's
    // probably better to just not give FTS results than strange errors when
    // everything else is working OK.
    LOG(WARNING) << "Text database is too new.";
    return false;
  }

  return CreateTables();
}

void TextDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void TextDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

bool TextDatabase::CreateTables() {
  // FTS table of page contents.
  if (!db_.DoesTableExist("pages")) {
    if (!db_.Execute("CREATE VIRTUAL TABLE pages USING fts3("
                     "TOKENIZE icu,"
                     "url LONGVARCHAR,"
                     "title LONGVARCHAR,"
                     "body LONGVARCHAR)"))
      return false;
  }

  // Non-FTS table containing URLs and times so we can efficiently find them
  // using a regular index (all FTS columns are special and are treated as
  // full-text-search, which is not what we want when retrieving this data).
  if (!db_.DoesTableExist("info")) {
    // Note that there is no point in creating an index over time. Since
    // we must always query the entire FTS table (it can not efficiently do
    // subsets), we will always end up doing that first, and joining the info
    // table off of that.
    if (!db_.Execute("CREATE TABLE info(time INTEGER NOT NULL)"))
      return false;
  }

  // Create the index.
  return db_.Execute("CREATE INDEX IF NOT EXISTS info_time ON info(time)");
}

bool TextDatabase::AddPageData(base::Time time,
                               const std::string& url,
                               const std::string& title,
                               const std::string& contents) {
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return false;

  // Add to the pages table.
  sql::Statement add_to_pages(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO pages (url, title, body) VALUES (?,?,?)"));
  add_to_pages.BindString(0, url);
  add_to_pages.BindString(1, title);
  add_to_pages.BindString(2, contents);
  if (!add_to_pages.Run())
    return false;

  int64 rowid = db_.GetLastInsertRowId();

  // Add to the info table with the same rowid.
  sql::Statement add_to_info(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO info (rowid, time) VALUES (?,?)"));
  add_to_info.BindInt64(0, rowid);
  add_to_info.BindInt64(1, time.ToInternalValue());

  if (!add_to_info.Run())
    return false;

  return committer.Commit();
}

void TextDatabase::DeletePageData(base::Time time, const std::string& url) {
  // First get all rows that match. Selecing on time (which has an index) allows
  // us to avoid brute-force searches on the full-text-index table (there will
  // generally be only one match per time).
  sql::Statement select_ids(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT info.rowid "
      "FROM info JOIN pages ON info.rowid = pages.rowid "
      "WHERE info.time=? AND pages.url=?"));
  select_ids.BindInt64(0, time.ToInternalValue());
  select_ids.BindString(1, url);

  std::set<int64> rows_to_delete;
  while (select_ids.Step())
    rows_to_delete.insert(select_ids.ColumnInt64(0));

  // Delete from the pages table.
  sql::Statement delete_page(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM pages WHERE rowid=?"));

  for (std::set<int64>::const_iterator i = rows_to_delete.begin();
       i != rows_to_delete.end(); ++i) {
    delete_page.BindInt64(0, *i);
    if (!delete_page.Run())
      return;
    delete_page.Reset(true);
  }

  // Delete from the info table.
  sql::Statement delete_info(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM info WHERE rowid=?"));

  for (std::set<int64>::const_iterator i = rows_to_delete.begin();
       i != rows_to_delete.end(); ++i) {
    delete_info.BindInt64(0, *i);
    if (!delete_info.Run())
      return;
    delete_info.Reset(true);
  }
}

void TextDatabase::Optimize() {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT OPTIMIZE(pages) FROM pages LIMIT 1"));
  statement.Run();
}

bool TextDatabase::GetTextMatches(const std::string& query,
                                  const QueryOptions& options,
                                  std::vector<Match>* results,
                                  URLSet* found_urls) {
  std::string sql =
      "SELECT info.rowid, url, title, time, offsets(pages), body FROM pages "
      "LEFT OUTER JOIN info ON pages.rowid = info.rowid WHERE ";
  sql += options.body_only ? "body " : "pages ";
  sql += "MATCH ? AND time >= ? AND (time < ?";
  if (!options.cursor.empty())
    sql += " OR (time = ? AND info.rowid < ?)";
  // Times may not be unique, so also sort by rowid to ensure a stable order.
  sql += ") ORDER BY time DESC, info.rowid DESC";

  // Generate unique IDs for the different variations of the statement,
  // so they don't share the same cached prepared statement.
  sql::StatementID body_only_id = SQL_FROM_HERE;
  sql::StatementID pages_id = SQL_FROM_HERE;
  sql::StatementID pages_with_cursor_id = SQL_FROM_HERE;

  // Ensure that cursor and body_only aren't both specified, because that
  // combination is not covered here.
  DCHECK(!options.body_only || options.cursor.empty());

  // Choose the correct statement ID based on the options.
  sql::StatementID statement_id = pages_id;
  if (options.body_only)
    statement_id = body_only_id;
  else if (!options.cursor.empty())
    statement_id = pages_with_cursor_id;

  sql::Statement statement(db_.GetCachedStatement(statement_id, sql.c_str()));

  int i = 0;
  statement.BindString(i++, query);
  statement.BindInt64(i++, options.EffectiveBeginTime());
  statement.BindInt64(i++, options.EffectiveEndTime());
  if (!options.cursor.empty()) {
    statement.BindInt64(i++, options.EffectiveEndTime());
    statement.BindInt64(i++, options.cursor.rowid_);
  }

  // |results| may not be initially empty, so keep track of how many were added
  // by this call.
  int result_count = 0;

  while (statement.Step()) {
    // TODO(brettw) allow canceling the query in the middle.
    // if (canceled_or_something)
    //   break;

    GURL url(statement.ColumnString(1));
    URLSet::const_iterator found_url = found_urls->find(url);
    if (found_url != found_urls->end())
      continue;  // Don't add this duplicate.

    if (options.max_count > 0 && ++result_count > options.max_count)
      break;

    // Fill the results into the vector (avoid copying the URL with Swap()).
    results->resize(results->size() + 1);
    Match& match = results->at(results->size() - 1);
    match.rowid = statement.ColumnInt64(0);
    match.url.Swap(&url);

    match.title = statement.ColumnString16(2);
    match.time = base::Time::FromInternalValue(statement.ColumnInt64(3));

    // Extract any matches in the title.
    std::string offsets_str = statement.ColumnString(4);
    Snippet::ExtractMatchPositions(offsets_str, kTitleColumnIndex,
                                   &match.title_match_positions);
    Snippet::ConvertMatchPositionsToWide(statement.ColumnString(2),
                                         &match.title_match_positions);

    // Extract the matches in the body.
    Snippet::MatchPositions match_positions;
    Snippet::ExtractMatchPositions(offsets_str, kBodyColumnIndex,
                                   &match_positions);

    // Compute the snippet based on those matches.
    std::string body = statement.ColumnString(5);
    match.snippet.ComputeSnippet(match_positions, body);
  }
  statement.Reset(true);
  return result_count > options.max_count;
}

}  // namespace history
