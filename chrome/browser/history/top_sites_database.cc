// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/top_sites_database.h"
#include "sql/connection.h"
#include "sql/transaction.h"

namespace history {

// From the version 1 to 2, one column was added. Old versions of Chrome
// should be able to read version 2 files just fine.
static const int kVersionNumber = 2;

TopSitesDatabase::TopSitesDatabase() : may_need_history_migration_(false) {
}

TopSitesDatabase::~TopSitesDatabase() {
}

bool TopSitesDatabase::Init(const base::FilePath& db_name) {
  bool file_existed = file_util::PathExists(db_name);

  if (!file_existed)
    may_need_history_migration_ = true;

  db_.reset(CreateDB(db_name));
  if (!db_.get())
    return false;

  bool does_meta_exist = sql::MetaTable::DoesTableExist(db_.get());
  if (!does_meta_exist && file_existed) {
    may_need_history_migration_ = true;

    // If the meta file doesn't exist, this version is old. We could remove all
    // the entries as they are no longer applicable, but it's safest to just
    // remove the file and start over.
    db_.reset(NULL);
    if (!file_util::Delete(db_name, false) &&
        !file_util::Delete(db_name, false)) {
      // Try to delete twice. If we can't, fail.
      LOG(ERROR) << "unable to delete old TopSites file";
      return false;
    }
    db_.reset(CreateDB(db_name));
    if (!db_.get())
      return false;
  }

  // Scope initialization in a transaction so we can't be partially
  // initialized.
  sql::Transaction transaction(db_.get());
  transaction.Begin();

  if (!meta_table_.Init(db_.get(), kVersionNumber, kVersionNumber))
    return false;

  if (!InitThumbnailTable())
    return false;

  if (meta_table_.GetVersionNumber() == 1) {
    if (!UpgradeToVersion2()) {
      LOG(WARNING) << "Unable to upgrade top sites database to version 2.";
      return false;
    }
  }

  // Version check.
  if (meta_table_.GetVersionNumber() != kVersionNumber)
    return false;

  // Initialization is complete.
  if (!transaction.Commit())
    return false;

  return true;
}

bool TopSitesDatabase::InitThumbnailTable() {
  if (!db_->DoesTableExist("thumbnails")) {
    if (!db_->Execute("CREATE TABLE thumbnails ("
                      "url LONGVARCHAR PRIMARY KEY,"
                      "url_rank INTEGER ,"
                      "title LONGVARCHAR,"
                      "thumbnail BLOB,"
                      "redirects LONGVARCHAR,"
                      "boring_score DOUBLE DEFAULT 1.0, "
                      "good_clipping INTEGER DEFAULT 0, "
                      "at_top INTEGER DEFAULT 0, "
                      "last_updated INTEGER DEFAULT 0, "
                      "load_completed INTEGER DEFAULT 0) ")) {
      LOG(WARNING) << db_->GetErrorMessage();
      return false;
    }
  }
  return true;
}

bool TopSitesDatabase::UpgradeToVersion2() {
  // Add 'load_completed' column.
  if (!db_->Execute(
          "ALTER TABLE thumbnails ADD load_completed INTEGER DEFAULT 0")) {
    NOTREACHED();
    return false;
  }
  meta_table_.SetVersionNumber(2);
  return true;
}

void TopSitesDatabase::GetPageThumbnails(MostVisitedURLList* urls,
                                         URLToImagesMap* thumbnails) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url, url_rank, title, thumbnail, redirects, "
      "boring_score, good_clipping, at_top, last_updated, load_completed "
      "FROM thumbnails ORDER BY url_rank "));

  if (!statement.is_valid()) {
    LOG(WARNING) << db_->GetErrorMessage();
    return;
  }

  urls->clear();
  thumbnails->clear();

  while (statement.Step()) {
    // Results are sorted by url_rank.
    MostVisitedURL url;
    GURL gurl(statement.ColumnString(0));
    url.url = gurl;
    url.title = statement.ColumnString16(2);
    std::string redirects = statement.ColumnString(4);
    SetRedirects(redirects, &url);
    urls->push_back(url);

    std::vector<unsigned char> data;
    statement.ColumnBlobAsVector(3, &data);
    Images thumbnail;
    if (!data.empty())
      thumbnail.thumbnail = base::RefCountedBytes::TakeVector(&data);
    thumbnail.thumbnail_score.boring_score = statement.ColumnDouble(5);
    thumbnail.thumbnail_score.good_clipping = statement.ColumnBool(6);
    thumbnail.thumbnail_score.at_top = statement.ColumnBool(7);
    thumbnail.thumbnail_score.time_at_snapshot =
        base::Time::FromInternalValue(statement.ColumnInt64(8));
    thumbnail.thumbnail_score.load_completed = statement.ColumnBool(9);

    (*thumbnails)[gurl] = thumbnail;
  }
}

// static
std::string TopSitesDatabase::GetRedirects(const MostVisitedURL& url) {
  std::vector<std::string> redirects;
  for (size_t i = 0; i < url.redirects.size(); i++)
    redirects.push_back(url.redirects[i].spec());
  return JoinString(redirects, ' ');
}

// static
void TopSitesDatabase::SetRedirects(const std::string& redirects,
                                    MostVisitedURL* url) {
  std::vector<std::string> redirects_vector;
  base::SplitStringAlongWhitespace(redirects, &redirects_vector);
  for (size_t i = 0; i < redirects_vector.size(); ++i)
    url->redirects.push_back(GURL(redirects_vector[i]));
}

void TopSitesDatabase::SetPageThumbnail(const MostVisitedURL& url,
                                            int new_rank,
                                            const Images& thumbnail) {
  sql::Transaction transaction(db_.get());
  transaction.Begin();

  int rank = GetURLRank(url);
  if (rank == -1) {
    AddPageThumbnail(url, new_rank, thumbnail);
  } else {
    UpdatePageRankNoTransaction(url, new_rank);
    UpdatePageThumbnail(url, thumbnail);
  }

  transaction.Commit();
}

bool TopSitesDatabase::UpdatePageThumbnail(
    const MostVisitedURL& url, const Images& thumbnail) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE thumbnails SET "
      "title = ?, thumbnail = ?, redirects = ?, "
      "boring_score = ?, good_clipping = ?, at_top = ?, last_updated = ?, "
      "load_completed = ? "
      "WHERE url = ? "));
  statement.BindString16(0, url.title);
  if (thumbnail.thumbnail.get() && thumbnail.thumbnail->front()) {
    statement.BindBlob(1, thumbnail.thumbnail->front(),
                       static_cast<int>(thumbnail.thumbnail->size()));
  }
  statement.BindString(2, GetRedirects(url));
  const ThumbnailScore& score = thumbnail.thumbnail_score;
  statement.BindDouble(3, score.boring_score);
  statement.BindBool(4, score.good_clipping);
  statement.BindBool(5, score.at_top);
  statement.BindInt64(6, score.time_at_snapshot.ToInternalValue());
  statement.BindBool(7, score.load_completed);
  statement.BindString(8, url.url.spec());

  return statement.Run();
}

void TopSitesDatabase::AddPageThumbnail(const MostVisitedURL& url,
                                            int new_rank,
                                            const Images& thumbnail) {
  int count = GetRowCount();

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO thumbnails "
      "(url, url_rank, title, thumbnail, redirects, "
      "boring_score, good_clipping, at_top, last_updated, load_completed) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindString(0, url.url.spec());
  statement.BindInt(1, count);  // Make it the last url.
  statement.BindString16(2, url.title);
  if (thumbnail.thumbnail.get() && thumbnail.thumbnail->front()) {
    statement.BindBlob(3, thumbnail.thumbnail->front(),
                       static_cast<int>(thumbnail.thumbnail->size()));
  }
  statement.BindString(4, GetRedirects(url));
  const ThumbnailScore& score = thumbnail.thumbnail_score;
  statement.BindDouble(5, score.boring_score);
  statement.BindBool(6, score.good_clipping);
  statement.BindBool(7, score.at_top);
  statement.BindInt64(8, score.time_at_snapshot.ToInternalValue());
  statement.BindBool(9, score.load_completed);
  if (!statement.Run())
    return;

  UpdatePageRankNoTransaction(url, new_rank);
}

void TopSitesDatabase::UpdatePageRank(const MostVisitedURL& url,
                                          int new_rank) {
  sql::Transaction transaction(db_.get());
  transaction.Begin();
  UpdatePageRankNoTransaction(url, new_rank);
  transaction.Commit();
}

// Caller should have a transaction open.
void TopSitesDatabase::UpdatePageRankNoTransaction(
    const MostVisitedURL& url, int new_rank) {
  int prev_rank = GetURLRank(url);
  if (prev_rank == -1) {
    LOG(WARNING) << "Updating rank of an unknown URL: " << url.url.spec();
    return;
  }

  // Shift the ranks.
  if (prev_rank > new_rank) {
    // Shift up
    sql::Statement shift_statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE thumbnails "
        "SET url_rank = url_rank + 1 "
        "WHERE url_rank >= ? AND url_rank < ?"));
    shift_statement.BindInt(0, new_rank);
    shift_statement.BindInt(1, prev_rank);
    shift_statement.Run();
  } else if (prev_rank < new_rank) {
    // Shift down
    sql::Statement shift_statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE thumbnails "
        "SET url_rank = url_rank - 1 "
        "WHERE url_rank > ? AND url_rank <= ?"));
    shift_statement.BindInt(0, prev_rank);
    shift_statement.BindInt(1, new_rank);
    shift_statement.Run();
  }

  // Set the url's rank.
  sql::Statement set_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE thumbnails "
      "SET url_rank = ? "
      "WHERE url == ?"));
  set_statement.BindInt(0, new_rank);
  set_statement.BindString(1, url.url.spec());
  set_statement.Run();
}

bool TopSitesDatabase::GetPageThumbnail(const GURL& url,
                                            Images* thumbnail) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT thumbnail, boring_score, good_clipping, at_top, last_updated "
      "FROM thumbnails WHERE url=?"));
  statement.BindString(0, url.spec());
  if (!statement.Step())
    return false;

  std::vector<unsigned char> data;
  statement.ColumnBlobAsVector(0, &data);
  thumbnail->thumbnail = base::RefCountedBytes::TakeVector(&data);
  thumbnail->thumbnail_score.boring_score = statement.ColumnDouble(1);
  thumbnail->thumbnail_score.good_clipping = statement.ColumnBool(2);
  thumbnail->thumbnail_score.at_top = statement.ColumnBool(3);
  thumbnail->thumbnail_score.time_at_snapshot =
      base::Time::FromInternalValue(statement.ColumnInt64(4));
  return true;
}

int TopSitesDatabase::GetRowCount() {
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT COUNT (url) FROM thumbnails"));
  if (select_statement.Step())
    return select_statement.ColumnInt(0);

  return 0;
}

int TopSitesDatabase::GetURLRank(const MostVisitedURL& url) {
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url_rank "
      "FROM thumbnails WHERE url=?"));
  select_statement.BindString(0, url.url.spec());
  if (select_statement.Step())
    return select_statement.ColumnInt(0);

  return -1;
}

// Remove the record for this URL. Returns true iff removed successfully.
bool TopSitesDatabase::RemoveURL(const MostVisitedURL& url) {
  int old_rank = GetURLRank(url);
  if (old_rank < 0)
    return false;

  sql::Transaction transaction(db_.get());
  transaction.Begin();
  // Decrement all following ranks.
  sql::Statement shift_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE thumbnails "
      "SET url_rank = url_rank - 1 "
      "WHERE url_rank > ?"));
  shift_statement.BindInt(0, old_rank);

  if (!shift_statement.Run())
    return false;

  sql::Statement delete_statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "DELETE FROM thumbnails WHERE url = ?"));
  delete_statement.BindString(0, url.url.spec());

  if (!delete_statement.Run())
    return false;

  return transaction.Commit();
}

sql::Connection* TopSitesDatabase::CreateDB(const base::FilePath& db_name) {
  scoped_ptr<sql::Connection> db(new sql::Connection());
  // Settings copied from ThumbnailDatabase.
  db->set_error_histogram_name("Sqlite.Thumbnail.Error");
  db->set_page_size(4096);
  db->set_cache_size(32);

  if (!db->Open(db_name)) {
    LOG(ERROR) << db->GetErrorMessage();
    return NULL;
  }

  return db.release();
}

}  // namespace history
