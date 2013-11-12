// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/top_sites_database.h"
#include "chrome/common/thumbnail_score.h"
#include "sql/connection.h"
#include "sql/transaction.h"

// Description of database table:
//
// thumbnails
//   url              URL of the sites for which we have a thumbnail.
//   url_rank         Index of the URL in that thumbnail, 0-based. The thumbnail
//                    with the highest rank will be the next one evicted. Forced
//                    thumbnails have a rank of -1.
//   title            The title to display under that thumbnail.
//   redirects        A space separated list of URLs that are known to redirect
//                    to this url.
//   boring_score     How "boring" that thumbnail is. See ThumbnailScore.
//   good_clipping    True if the thumbnail was clipped from the bottom, keeping
//                    the entire width of the window. See ThumbnailScore.
//   at_top           True if the thumbnail was captured at the top of the
//                    website.
//   last_updated     The time at which this thumbnail was last updated.
//   load_completed   True if the thumbnail was captured after the page load was
//                    completed.
//   last_forced      If this is a forced thumbnail, records the last time it
//                    was forced. If it's not a forced thumbnail, 0.

namespace history {

// TODO(beaudoin): Fill revision/date details of Version 3 after landing.
// Version 3:  by beaudoin@chromium.org
// Version 2: eb0b24e6/r87284 by satorux@chromium.org on 2011-05-31
// Version 1: 809cc4d8/r64072 by sky@chromium.org on 2010-10-27

// From the version 1 to 2, one column was added. Old versions of Chrome
// should be able to read version 2 files just fine. Same thing for version 2
// to 3.
// NOTE(shess): When changing the version, add a new golden file for
// the new version and a test to verify that Init() works with it.
static const int kVersionNumber = 3;

TopSitesDatabase::TopSitesDatabase() {
}

TopSitesDatabase::~TopSitesDatabase() {
}

bool TopSitesDatabase::Init(const base::FilePath& db_name) {
  bool file_existed = base::PathExists(db_name);

  db_.reset(CreateDB(db_name));
  if (!db_)
    return false;

  bool does_meta_exist = sql::MetaTable::DoesTableExist(db_.get());
  if (!does_meta_exist && file_existed) {
    // If the meta file doesn't exist, this version is old. We could remove all
    // the entries as they are no longer applicable, but it's safest to just
    // remove the file and start over.
    db_.reset(NULL);
    if (!sql::Connection::Delete(db_name)) {
      LOG(ERROR) << "unable to delete old TopSites file";
      return false;
    }
    db_.reset(CreateDB(db_name));
    if (!db_)
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

  if (meta_table_.GetVersionNumber() == 2) {
    if (!UpgradeToVersion3()) {
      LOG(WARNING) << "Unable to upgrade top sites database to version 3.";
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
                      "url_rank INTEGER,"
                      "title LONGVARCHAR,"
                      "thumbnail BLOB,"
                      "redirects LONGVARCHAR,"
                      "boring_score DOUBLE DEFAULT 1.0,"
                      "good_clipping INTEGER DEFAULT 0,"
                      "at_top INTEGER DEFAULT 0,"
                      "last_updated INTEGER DEFAULT 0,"
                      "load_completed INTEGER DEFAULT 0,"
                      "last_forced INTEGER DEFAULT 0)")) {
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

bool TopSitesDatabase::UpgradeToVersion3() {
  // Add 'last_forced' column.
  if (!db_->Execute(
          "ALTER TABLE thumbnails ADD last_forced INTEGER DEFAULT 0")) {
    NOTREACHED();
    return false;
  }
  meta_table_.SetVersionNumber(3);
  return true;
}

void TopSitesDatabase::GetPageThumbnails(MostVisitedURLList* urls,
                                         URLToImagesMap* thumbnails) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url, url_rank, title, thumbnail, redirects, "
      "boring_score, good_clipping, at_top, last_updated, load_completed, "
      "last_forced FROM thumbnails ORDER BY url_rank, last_forced"));

  if (!statement.is_valid()) {
    LOG(WARNING) << db_->GetErrorMessage();
    return;
  }

  urls->clear();
  thumbnails->clear();

  while (statement.Step()) {
    // Results are sorted by url_rank. For forced thumbnails with url_rank = -1,
    // thumbnails are sorted by last_forced.
    MostVisitedURL url;
    GURL gurl(statement.ColumnString(0));
    url.url = gurl;
    url.title = statement.ColumnString16(2);
    url.last_forced_time =
        base::Time::FromInternalValue(statement.ColumnInt64(10));
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
  if (rank == kRankOfNonExistingURL) {
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
      "load_completed = ?, last_forced = ?"
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
  statement.BindInt64(8, url.last_forced_time.ToInternalValue());
  statement.BindString(9, url.url.spec());

  return statement.Run();
}

void TopSitesDatabase::AddPageThumbnail(const MostVisitedURL& url,
                                        int new_rank,
                                        const Images& thumbnail) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO thumbnails "
      "(url, url_rank, title, thumbnail, redirects, "
      "boring_score, good_clipping, at_top, last_updated, load_completed, "
      "last_forced) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindString(0, url.url.spec());
  statement.BindInt(1, kRankOfForcedURL);  // Fist make it a forced thumbnail.
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
  int64 last_forced = url.last_forced_time.ToInternalValue();
  DCHECK((last_forced == 0) == (new_rank != kRankOfForcedURL))
      << "Thumbnail without a forced time stamp has a forced rank, or the "
      << "opposite.";
  statement.BindInt64(10, last_forced);
  if (!statement.Run())
    return;

  // Update rank if this is not a forced thumbnail.
  if (new_rank != kRankOfForcedURL)
    UpdatePageRankNoTransaction(url, new_rank);
}

void TopSitesDatabase::UpdatePageRank(const MostVisitedURL& url,
                                      int new_rank) {
  DCHECK((url.last_forced_time.ToInternalValue() == 0) ==
         (new_rank != kRankOfForcedURL))
      << "Thumbnail without a forced time stamp has a forced rank, or the "
      << "opposite.";
  sql::Transaction transaction(db_.get());
  transaction.Begin();
  UpdatePageRankNoTransaction(url, new_rank);
  transaction.Commit();
}

// Caller should have a transaction open.
void TopSitesDatabase::UpdatePageRankNoTransaction(
    const MostVisitedURL& url, int new_rank) {
  DCHECK_GT(db_->transaction_nesting(), 0);
  DCHECK((url.last_forced_time.is_null()) == (new_rank != kRankOfForcedURL))
      << "Thumbnail without a forced time stamp has a forced rank, or the "
      << "opposite.";

  int prev_rank = GetURLRank(url);
  if (prev_rank == kRankOfNonExistingURL) {
    LOG(WARNING) << "Updating rank of an unknown URL: " << url.url.spec();
    return;
  }

  // Shift the ranks.
  if (prev_rank > new_rank) {
    if (new_rank == kRankOfForcedURL) {
      // From non-forced to forced, shift down.
      // Example: 2 -> -1
      // -1, -1, -1, 0, 1, [2 -> -1], [3 -> 2], [4 -> 3]
      sql::Statement shift_statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "UPDATE thumbnails "
          "SET url_rank = url_rank - 1 "
          "WHERE url_rank > ?"));
      shift_statement.BindInt(0, prev_rank);
      shift_statement.Run();
    } else {
      // From non-forced to non-forced, shift up.
      // Example: 3 -> 1
      // -1, -1, -1, 0, [1 -> 2], [2 -> 3], [3 -> 1], 4
      sql::Statement shift_statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "UPDATE thumbnails "
          "SET url_rank = url_rank + 1 "
          "WHERE url_rank >= ? AND url_rank < ?"));
      shift_statement.BindInt(0, new_rank);
      shift_statement.BindInt(1, prev_rank);
      shift_statement.Run();
    }
  } else if (prev_rank < new_rank) {
    if (prev_rank == kRankOfForcedURL) {
      // From non-forced to forced, shift up.
      // Example: -1 -> 2
      // -1, [-1 -> 2], -1, 0, 1, [2 -> 3], [3 -> 4], [4 -> 5]
      sql::Statement shift_statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "UPDATE thumbnails "
          "SET url_rank = url_rank + 1 "
          "WHERE url_rank >= ?"));
      shift_statement.BindInt(0, new_rank);
      shift_statement.Run();
    } else {
      // From non-forced to non-forced, shift down.
      // Example: 1 -> 3.
      // -1, -1, -1, 0, [1 -> 3], [2 -> 1], [3 -> 2], 4
      sql::Statement shift_statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "UPDATE thumbnails "
          "SET url_rank = url_rank - 1 "
          "WHERE url_rank > ? AND url_rank <= ?"));
      shift_statement.BindInt(0, prev_rank);
      shift_statement.BindInt(1, new_rank);
      shift_statement.Run();
    }
  }

  // Set the url's rank and last_forced, since the latter changes when a URL
  // goes from forced to non-forced and vice-versa.
  sql::Statement set_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE thumbnails "
      "SET url_rank = ?, last_forced = ? "
      "WHERE url == ?"));
  set_statement.BindInt(0, new_rank);
  set_statement.BindInt64(1, url.last_forced_time.ToInternalValue());
  set_statement.BindString(2, url.url.spec());
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

int TopSitesDatabase::GetURLRank(const MostVisitedURL& url) {
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url_rank "
      "FROM thumbnails WHERE url=?"));
  select_statement.BindString(0, url.url.spec());
  if (select_statement.Step())
    return select_statement.ColumnInt(0);

  return kRankOfNonExistingURL;
}

// Remove the record for this URL. Returns true iff removed successfully.
bool TopSitesDatabase::RemoveURL(const MostVisitedURL& url) {
  int old_rank = GetURLRank(url);
  if (old_rank == kRankOfNonExistingURL)
    return false;

  sql::Transaction transaction(db_.get());
  transaction.Begin();
  if (old_rank != kRankOfForcedURL) {
    // Decrement all following ranks.
    sql::Statement shift_statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE thumbnails "
        "SET url_rank = url_rank - 1 "
        "WHERE url_rank > ?"));
    shift_statement.BindInt(0, old_rank);

    if (!shift_statement.Run())
      return false;
  }

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
  db->set_histogram_tag("TopSites");
  db->set_page_size(4096);
  db->set_cache_size(32);

  if (!db->Open(db_name)) {
    LOG(ERROR) << db->GetErrorMessage();
    return NULL;
  }

  return db.release();
}

}  // namespace history
