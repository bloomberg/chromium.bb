// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/transaction.h"
#include "base/string_util.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/top_sites_database.h"

namespace history {

TopSitesDatabaseImpl::TopSitesDatabaseImpl() {
}

bool TopSitesDatabaseImpl::Init(const FilePath& db_name) {
  // Settings copied from ThumbnailDatabase.
  db_.set_error_delegate(GetErrorHandlerForThumbnailDb());
  db_.set_page_size(4096);
  db_.set_cache_size(64);

  if (!db_.Open(db_name)) {
    LOG(WARNING) << db_.GetErrorMessage();
    return false;
  }

  return InitThumbnailTable();
}

bool TopSitesDatabaseImpl::InitThumbnailTable() {
  if (!db_.DoesTableExist("thumbnails")) {
    if (!db_.Execute("CREATE TABLE thumbnails ("
                     "url LONGVARCHAR PRIMARY KEY,"
                     "url_rank INTEGER ,"
                     "title LONGVARCHAR,"
                     "thumbnail BLOB,"
                     "redirects LONGVARCHAR,"
                     "boring_score DOUBLE DEFAULT 1.0, "
                     "good_clipping INTEGER DEFAULT 0, "
                     "at_top INTEGER DEFAULT 0, "
                     "last_updated INTEGER DEFAULT 0) ")) {
      LOG(WARNING) << db_.GetErrorMessage();
      return false;
    }
  }
  return true;
}

MostVisitedURLList TopSitesDatabaseImpl::GetTopURLs() {
  MostVisitedURLList result;
  sql::Statement select_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url, url_rank, title, redirects "
      "FROM thumbnails ORDER BY url_rank"));

  if (!select_statement) {
    LOG(WARNING) << db_.GetErrorMessage();
    return result;
  }

  int row_count = 0;
  int max_rank = -1;  // -1 is for the case of empty DB.
  while (select_statement.Step()) {
    // Results are sorted by url_rank.
    MostVisitedURL url;
    GURL gurl(select_statement.ColumnString(0));
    url.url = gurl;
    url.title = select_statement.ColumnString16(2);
    std::string redirects = select_statement.ColumnString(3);
    SetRedirects(redirects, &url);
    result.push_back(url);
  }
  DCHECK(select_statement.Succeeded());
  DCHECK_EQ(max_rank + 1, row_count);

  return result;
}

// static
std::string TopSitesDatabaseImpl::GetRedirects(const MostVisitedURL& url) {
  std::vector<std::string> redirects;
  for (size_t i = 0; i < url.redirects.size(); i++)
    redirects.push_back(url.redirects[i].spec());
  return JoinString(redirects, ' ');
}

// static
void TopSitesDatabaseImpl::SetRedirects(const std::string& redirects,
                                        MostVisitedURL* url) {
  std::vector<std::string> redirects_vector;
  SplitStringAlongWhitespace(redirects, &redirects_vector);
  for (size_t i = 0; i < redirects_vector.size(); i++)
    url->redirects.push_back(GURL(redirects_vector[i]));
}


void TopSitesDatabaseImpl::SetPageThumbnail(const MostVisitedURL& url,
                                            int new_rank,
                                            const TopSites::Images& thumbnail) {
  sql::Transaction transaction(&db_);
  transaction.Begin();

  int prev_rank = GetURLRank(url);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO thumbnails "
      "(url, url_rank, title, thumbnail, redirects, "
      "boring_score, good_clipping, at_top, last_updated) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  if (!statement)
    return;

  statement.BindString(0, url.url.spec());
  statement.BindInt(1, -1);  // Temporary value
  statement.BindString16(2, url.title);
  if (thumbnail.thumbnail.get()) {
    statement.BindBlob(3, &thumbnail.thumbnail->data.front(),
                       static_cast<int>(thumbnail.thumbnail->data.size()));
  }
  statement.BindString(4, GetRedirects(url));
  const ThumbnailScore& score = thumbnail.thumbnail_score;
  statement.BindDouble(5, score.boring_score);
  statement.BindBool(6, score.good_clipping);
  statement.BindBool(7, score.at_top);
  statement.BindInt64(8, score.time_at_snapshot.ToInternalValue());

  if (!statement.Run())
    NOTREACHED() << db_.GetErrorMessage();

  // Shift the ranks.
  if (prev_rank == -1) {
    // Shift up
    sql::Statement shift_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE thumbnails "
        "SET url_rank = url_rank + 1 WHERE url_rank >= ?"));
    shift_statement.BindInt(0, new_rank);
    if (shift_statement)
      shift_statement.Run();
  } else if (prev_rank > new_rank) {
    // Shift up
    sql::Statement shift_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE thumbnails "
        "SET url_rank = url_rank + 1 "
        "WHERE url_rank >= ? AND url_rank < ?"));
    shift_statement.BindInt(0, new_rank);
    shift_statement.BindInt(1, prev_rank);
    if (shift_statement)
      shift_statement.Run();
  } else if (prev_rank < new_rank) {
    // Shift down
    sql::Statement shift_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE thumbnails "
        "SET url_rank = url_rank - 1 "
        "WHERE url_rank > ? AND url_rank <= ?"));
    shift_statement.BindInt(0, prev_rank);
    shift_statement.BindInt(1, new_rank);
    if (shift_statement)
      shift_statement.Run();
  }

  // Set the real rank.
  sql::Statement set_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE thumbnails "
      "SET url_rank = ? "
      "WHERE url_rank == -1"));
  set_statement.BindInt(0, new_rank);
  if (set_statement)
    set_statement.Run();
  transaction.Commit();
}

bool TopSitesDatabaseImpl::GetPageThumbnail(const MostVisitedURL& url,
                                            TopSites::Images* thumbnail) {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT thumbnail, boring_score, good_clipping, at_top, last_updated "
      "FROM thumbnails WHERE url=?"));

  if (!statement) {
    LOG(WARNING) << db_.GetErrorMessage();
    return false;
  }

  statement.BindString(0, url.url.spec());
  if (!statement.Step())
    return false;

  std::vector<unsigned char> data;
  statement.ColumnBlobAsVector(0, &data);
  thumbnail->thumbnail = RefCountedBytes::TakeVector(&data);
  thumbnail->thumbnail_score.boring_score = statement.ColumnDouble(1);
  thumbnail->thumbnail_score.good_clipping = statement.ColumnBool(2);
  thumbnail->thumbnail_score.at_top = statement.ColumnBool(3);
  thumbnail->thumbnail_score.time_at_snapshot =
      base::Time::FromInternalValue(statement.ColumnInt64(4));
  return true;
}

int TopSitesDatabaseImpl::GetURLRank(const MostVisitedURL& url) {
  int result = -1;
  sql::Statement select_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url_rank "
      "FROM thumbnails WHERE url=?"));
  if (!select_statement) {
    LOG(WARNING) << db_.GetErrorMessage();
    return result;
  }

  select_statement.BindString(0, url.url.spec());
  if (select_statement.Step())
    result = select_statement.ColumnInt(0);

  return result;
}

// Remove the record for this URL. Returns true iff removed successfully.
bool TopSitesDatabaseImpl::RemoveURL(const MostVisitedURL& url) {
  int old_rank = GetURLRank(url);
  sql::Transaction transaction(&db_);
  transaction.Begin();
  // Decrement all following ranks.
  sql::Statement shift_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE thumbnails "
      "SET url_rank = url_rank - 1 "
      "WHERE url_rank > ?"));
  if (!shift_statement)
    return false;
  shift_statement.BindInt(0, old_rank);
  shift_statement.Run();

  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM thumbnails WHERE url = ?"));
  if (!delete_statement)
    return false;
  delete_statement.BindString(0, url.url.spec());
  delete_statement.Run();

  return transaction.Commit();
}

}  // namespace history
