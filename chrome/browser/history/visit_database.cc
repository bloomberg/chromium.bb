// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_database.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/page_transition_types.h"
#include "sql/statement.h"

// Rows, in order, of the visit table.
#define HISTORY_VISIT_ROW_FIELDS \
    " id,url,visit_time,from_visit,transition,segment_id,is_indexed," \
    "visit_duration "

namespace history {

VisitDatabase::VisitDatabase() {
}

VisitDatabase::~VisitDatabase() {
}

bool VisitDatabase::InitVisitTable() {
  if (!GetDB().DoesTableExist("visits")) {
    if (!GetDB().Execute("CREATE TABLE visits("
        "id INTEGER PRIMARY KEY,"
        "url INTEGER NOT NULL," // key of the URL this corresponds to
        "visit_time INTEGER NOT NULL,"
        "from_visit INTEGER,"
        "transition INTEGER DEFAULT 0 NOT NULL,"
        "segment_id INTEGER,"
        // True when we have indexed data for this visit.
        "is_indexed BOOLEAN,"
        "visit_duration INTEGER DEFAULT 0 NOT NULL)"))
      return false;
  } else if (!GetDB().DoesColumnExist("visits", "is_indexed")) {
    // Old versions don't have the is_indexed column, we can just add that and
    // not worry about different database revisions, since old ones will
    // continue to work.
    //
    // TODO(brettw) this should be removed once we think everybody has been
    // updated (added early Mar 2008).
    if (!GetDB().Execute("ALTER TABLE visits ADD COLUMN is_indexed BOOLEAN"))
      return false;
  }

  // Visit source table contains the source information for all the visits. To
  // save space, we do not record those user browsed visits which would be the
  // majority in this table. Only other sources are recorded.
  // Due to the tight relationship between visit_source and visits table, they
  // should be created and dropped at the same time.
  if (!GetDB().DoesTableExist("visit_source")) {
    if (!GetDB().Execute("CREATE TABLE visit_source("
                         "id INTEGER PRIMARY KEY,source INTEGER NOT NULL)"))
        return false;
  }

  // Index over url so we can quickly find visits for a page.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS visits_url_index ON visits (url)"))
    return false;

  // Create an index over from visits so that we can efficiently find
  // referrers and redirects.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS visits_from_index ON "
          "visits (from_visit)"))
    return false;

  // Create an index over time so that we can efficiently find the visits in a
  // given time range (most history views are time-based).
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS visits_time_index ON "
          "visits (visit_time)"))
    return false;

  return true;
}

bool VisitDatabase::DropVisitTable() {
  // This will also drop the indices over the table.
  return
      GetDB().Execute("DROP TABLE IF EXISTS visit_source") &&
      GetDB().Execute("DROP TABLE visits");
}

// Must be in sync with HISTORY_VISIT_ROW_FIELDS.
// static
void VisitDatabase::FillVisitRow(sql::Statement& statement, VisitRow* visit) {
  visit->visit_id = statement.ColumnInt64(0);
  visit->url_id = statement.ColumnInt64(1);
  visit->visit_time = base::Time::FromInternalValue(statement.ColumnInt64(2));
  visit->referring_visit = statement.ColumnInt64(3);
  visit->transition = content::PageTransitionFromInt(statement.ColumnInt(4));
  visit->segment_id = statement.ColumnInt64(5);
  visit->is_indexed = !!statement.ColumnInt(6);
  visit->visit_duration =
      base::TimeDelta::FromInternalValue(statement.ColumnInt64(7));
}

// static
bool VisitDatabase::FillVisitVector(sql::Statement& statement,
                                    VisitVector* visits) {
  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    history::VisitRow visit;
    FillVisitRow(statement, &visit);
    visits->push_back(visit);
  }

  return statement.Succeeded();
}

VisitID VisitDatabase::AddVisit(VisitRow* visit, VisitSource source) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO visits "
      "(url, visit_time, from_visit, transition, segment_id, is_indexed, "
      "visit_duration) VALUES (?,?,?,?,?,?,?)"));
  statement.BindInt64(0, visit->url_id);
  statement.BindInt64(1, visit->visit_time.ToInternalValue());
  statement.BindInt64(2, visit->referring_visit);
  statement.BindInt64(3, visit->transition);
  statement.BindInt64(4, visit->segment_id);
  statement.BindInt64(5, visit->is_indexed);
  statement.BindInt64(6, visit->visit_duration.ToInternalValue());

  if (!statement.Run()) {
    VLOG(0) << "Failed to execute visit insert statement:  "
            << "url_id = " << visit->url_id;
    return 0;
  }

  visit->visit_id = GetDB().GetLastInsertRowId();

  if (source != SOURCE_BROWSED) {
    // Record the source of this visit when it is not browsed.
    sql::Statement statement1(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "INSERT INTO visit_source (id, source) VALUES (?,?)"));
    statement1.BindInt64(0, visit->visit_id);
    statement1.BindInt64(1, source);

    if (!statement1.Run()) {
      VLOG(0) << "Failed to execute visit_source insert statement:  "
              << "id = " << visit->visit_id;
      return 0;
    }
  }

  return visit->visit_id;
}

void VisitDatabase::DeleteVisit(const VisitRow& visit) {
  // Patch around this visit. Any visits that this went to will now have their
  // "source" be the deleted visit's source.
  sql::Statement update_chain(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE visits SET from_visit=? WHERE from_visit=?"));
  update_chain.BindInt64(0, visit.referring_visit);
  update_chain.BindInt64(1, visit.visit_id);
  if (!update_chain.Run())
    return;

  // Now delete the actual visit.
  sql::Statement del(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM visits WHERE id=?"));
  del.BindInt64(0, visit.visit_id);
  if (!del.Run())
    return;

  // Try to delete the entry in visit_source table as well.
  // If the visit was browsed, there is no corresponding entry in visit_source
  // table, and nothing will be deleted.
  del.Assign(GetDB().GetCachedStatement(SQL_FROM_HERE,
             "DELETE FROM visit_source WHERE id=?"));
  del.BindInt64(0, visit.visit_id);
  del.Run();
}

bool VisitDatabase::GetRowForVisit(VisitID visit_id, VisitRow* out_visit) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits WHERE id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return false;

  FillVisitRow(statement, out_visit);

  // We got a different visit than we asked for, something is wrong.
  DCHECK_EQ(visit_id, out_visit->visit_id);
  if (visit_id != out_visit->visit_id)
    return false;

  return true;
}

bool VisitDatabase::UpdateVisitRow(const VisitRow& visit) {
  // Don't store inconsistent data to the database.
  DCHECK_NE(visit.visit_id, visit.referring_visit);
  if (visit.visit_id == visit.referring_visit)
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE visits SET "
      "url=?,visit_time=?,from_visit=?,transition=?,segment_id=?,is_indexed=?,"
      "visit_duration=? WHERE id=?"));
  statement.BindInt64(0, visit.url_id);
  statement.BindInt64(1, visit.visit_time.ToInternalValue());
  statement.BindInt64(2, visit.referring_visit);
  statement.BindInt64(3, visit.transition);
  statement.BindInt64(4, visit.segment_id);
  statement.BindInt64(5, visit.is_indexed);
  statement.BindInt64(6, visit.visit_duration.ToInternalValue());
  statement.BindInt64(7, visit.visit_id);

  return statement.Run();
}

bool VisitDatabase::GetVisitsForURL(URLID url_id, VisitVector* visits) {
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE url=? "
      "ORDER BY visit_time ASC"));
  statement.BindInt64(0, url_id);
  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetIndexedVisitsForURL(URLID url_id, VisitVector* visits) {
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE url=? AND is_indexed=1"));
  statement.BindInt64(0, url_id);
  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetAllVisitsInRange(base::Time begin_time,
                                        base::Time end_time,
                                        int max_results,
                                        VisitVector* visits) {
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ?"
      "ORDER BY visit_time LIMIT ?"));

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64 end = end_time.ToInternalValue();
  statement.BindInt64(0, begin_time.ToInternalValue());
  statement.BindInt64(1, end ? end : std::numeric_limits<int64>::max());
  statement.BindInt64(2,
      max_results ? max_results : std::numeric_limits<int64>::max());

  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetVisitsInRangeForTransition(
    base::Time begin_time,
    base::Time end_time,
    int max_results,
    content::PageTransition transition,
    VisitVector* visits) {
  DCHECK(visits);
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ? "
      "AND (transition & ?) == ?"
      "ORDER BY visit_time LIMIT ?"));

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64 end = end_time.ToInternalValue();
  statement.BindInt64(0, begin_time.ToInternalValue());
  statement.BindInt64(1, end ? end : std::numeric_limits<int64>::max());
  statement.BindInt(2, content::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(3, transition);
  statement.BindInt64(4,
      max_results ? max_results : std::numeric_limits<int64>::max());

  return FillVisitVector(statement, visits);
}

void VisitDatabase::GetVisibleVisitsInRange(base::Time begin_time,
                                            base::Time end_time,
                                            int max_count,
                                            VisitVector* visits,
                                            bool unique) {
  visits->clear();
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ? "
      "AND (transition & ?) != 0 "  // CHAIN_END
      "AND (transition & ?) NOT IN (?, ?, ?) "  // NO SUBFRAME or
                                                // KEYWORD_GENERATED
      "ORDER BY visit_time DESC, id DESC"));

  // Note that we use min/max values for querying unlimited ranges of time using
  // the same statement. Since the time has an index, this will be about the
  // same amount of work as just doing a query for everything with no qualifier.
  int64 end = end_time.ToInternalValue();
  statement.BindInt64(0, begin_time.ToInternalValue());
  statement.BindInt64(1, end ? end : std::numeric_limits<int64>::max());
  statement.BindInt(2, content::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt(3, content::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(4, content::PAGE_TRANSITION_AUTO_SUBFRAME);
  statement.BindInt(5, content::PAGE_TRANSITION_MANUAL_SUBFRAME);
  statement.BindInt(6, content::PAGE_TRANSITION_KEYWORD_GENERATED);

  std::set<URLID> found_urls;
  while (statement.Step()) {
    VisitRow visit;
    FillVisitRow(statement, &visit);
    if (unique) {
      // Make sure the URL this visit corresponds to is unique.
      if (found_urls.find(visit.url_id) != found_urls.end())
        continue;
      found_urls.insert(visit.url_id);
    }
    visits->push_back(visit);

    if (max_count > 0 && static_cast<int>(visits->size()) >= max_count)
      break;
  }
}

void VisitDatabase::GetVisibleVisitsDuringTimes(const VisitFilter& time_filter,
                                                int max_results,
                                                VisitVector* visits) {
  visits->clear();
  if (max_results)
    visits->reserve(max_results);
  for (VisitFilter::TimeVector::const_iterator it = time_filter.times().begin();
       it != time_filter.times().end(); ++it) {
    VisitVector v;
    GetVisibleVisitsInRange(it->first, it->second, max_results, &v, false);
    size_t take_only = 0;
    if (max_results &&
        static_cast<int>(visits->size() + v.size()) > max_results) {
      take_only = max_results - visits->size();
    }

    visits->insert(visits->end(),
                   v.begin(), take_only ? v.begin() + take_only : v.end());
    if (max_results && static_cast<int>(visits->size()) == max_results)
      return;
  }
}

VisitID VisitDatabase::GetMostRecentVisitForURL(URLID url_id,
                                                VisitRow* visit_row) {
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
      "WHERE url=? "
      "ORDER BY visit_time DESC, id DESC "
      "LIMIT 1"));
  statement.BindInt64(0, url_id);
  if (!statement.Step())
    return 0;  // No visits for this URL.

  if (visit_row) {
    FillVisitRow(statement, visit_row);
    return visit_row->visit_id;
  }
  return statement.ColumnInt64(0);
}

bool VisitDatabase::GetMostRecentVisitsForURL(URLID url_id,
                                              int max_results,
                                              VisitVector* visits) {
  visits->clear();

  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE url=? "
      "ORDER BY visit_time DESC, id DESC "
      "LIMIT ?"));
  statement.BindInt64(0, url_id);
  statement.BindInt(1, max_results);

  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetRedirectFromVisit(VisitID from_visit,
                                         VisitID* to_visit,
                                         GURL* to_url) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT v.id,u.url "
      "FROM visits v JOIN urls u ON v.url = u.id "
      "WHERE v.from_visit = ? "
      "AND (v.transition & ?) != 0"));  // IS_REDIRECT_MASK
  statement.BindInt64(0, from_visit);
  statement.BindInt(1, content::PAGE_TRANSITION_IS_REDIRECT_MASK);

  if (!statement.Step())
    return false;  // No redirect from this visit. (Or SQL error)
  if (to_visit)
    *to_visit = statement.ColumnInt64(0);
  if (to_url)
    *to_url = GURL(statement.ColumnString(1));
  return true;
}

bool VisitDatabase::GetRedirectToVisit(VisitID to_visit,
                                       VisitID* from_visit,
                                       GURL* from_url) {
  VisitRow row;
  if (!GetRowForVisit(to_visit, &row))
    return false;

  if (from_visit)
    *from_visit = row.referring_visit;

  if (from_url) {
    sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "SELECT u.url "
        "FROM visits v JOIN urls u ON v.url = u.id "
        "WHERE v.id = ?"));
    statement.BindInt64(0, row.referring_visit);

    if (!statement.Step())
      return false;

    *from_url = GURL(statement.ColumnString(0));
  }
  return true;
}

bool VisitDatabase::GetVisibleVisitCountToHost(const GURL& url,
                                               int* count,
                                               base::Time* first_visit) {
  if (!url.SchemeIs(chrome::kHttpScheme) && !url.SchemeIs(chrome::kHttpsScheme))
    return false;

  // We need to search for URLs with a matching host/port. One way to query for
  // this is to use the LIKE operator, eg 'url LIKE http://google.com/%'. This
  // is inefficient though in that it doesn't use the index and each entry must
  // be visited. The same query can be executed by using >= and < operator.
  // The query becomes:
  // 'url >= http://google.com/' and url < http://google.com0'.
  // 0 is used as it is one character greater than '/'.
  const std::string host_query_min = url.GetOrigin().spec();
  if (host_query_min.empty())
    return false;

  // We also want to restrict ourselves to main frame navigations that are not
  // in the middle of redirect chains, hence the transition checks.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT MIN(v.visit_time), COUNT(*) "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE u.url >= ? AND u.url < ? "
      "AND (transition & ?) != 0 "
      "AND (transition & ?) NOT IN (?, ?, ?)"));
  statement.BindString(0, host_query_min);
  statement.BindString(1,
      host_query_min.substr(0, host_query_min.size() - 1) + '0');
  statement.BindInt(2, content::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt(3, content::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(4, content::PAGE_TRANSITION_AUTO_SUBFRAME);
  statement.BindInt(5, content::PAGE_TRANSITION_MANUAL_SUBFRAME);
  statement.BindInt(6, content::PAGE_TRANSITION_KEYWORD_GENERATED);

  if (!statement.Step()) {
    // We've never been to this page before.
    *count = 0;
    return true;
  }

  if (!statement.Succeeded())
    return false;

  *first_visit = base::Time::FromInternalValue(statement.ColumnInt64(0));
  *count = statement.ColumnInt(1);
  return true;
}

bool VisitDatabase::GetStartDate(base::Time* first_visit) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT MIN(visit_time) FROM visits WHERE visit_time != 0"));
  if (!statement.Step() || statement.ColumnInt64(0) == 0) {
    *first_visit = base::Time::Now();
    return false;
  }
  *first_visit = base::Time::FromInternalValue(statement.ColumnInt64(0));
  return true;
}

void VisitDatabase::GetVisitsSource(const VisitVector& visits,
                                    VisitSourceMap* sources) {
  DCHECK(sources);
  sources->clear();

  // We query the source in batch. Here defines the batch size.
  const size_t batch_size = 500;
  size_t visits_size = visits.size();

  size_t start_index = 0, end_index = 0;
  while (end_index < visits_size) {
    start_index = end_index;
    end_index = end_index + batch_size < visits_size ? end_index + batch_size
                                                     : visits_size;

    // Compose the sql statement with a list of ids.
    std::string sql = "SELECT id,source FROM visit_source ";
    sql.append("WHERE id IN (");
    // Append all the ids in the statement.
    for (size_t j = start_index; j < end_index; j++) {
      if (j != start_index)
        sql.push_back(',');
      sql.append(base::Int64ToString(visits[j].visit_id));
    }
    sql.append(") ORDER BY id");
    sql::Statement statement(GetDB().GetUniqueStatement(sql.c_str()));

    // Get the source entries out of the query result.
    while (statement.Step()) {
      std::pair<VisitID, VisitSource> source_entry(statement.ColumnInt64(0),
          static_cast<VisitSource>(statement.ColumnInt(1)));
      sources->insert(source_entry);
    }
  }
}

bool VisitDatabase::MigrateVisitsWithoutDuration() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED() << " Visits table should exist before migration";
    return false;
  }

  if (!GetDB().DoesColumnExist("visits", "visit_duration")) {
    // Old versions don't have the visit_duration column, we modify the table
    // to add that field.
    if (!GetDB().Execute("ALTER TABLE visits "
        "ADD COLUMN visit_duration INTEGER DEFAULT 0 NOT NULL"))
      return false;
  }
  return true;
}

void VisitDatabase::GetBriefVisitInfoOfMostRecentVisits(
    int max_visits,
    std::vector<BriefVisitInfo>* result_vector) {
  result_vector->clear();

  sql::Statement statement(GetDB().GetUniqueStatement(
      "SELECT url,visit_time,transition FROM visits "
      "ORDER BY id DESC LIMIT ?"));

  statement.BindInt64(0, max_visits);

  if (!statement.is_valid())
    return;

  while (statement.Step()) {
    BriefVisitInfo info;
    info.url_id = statement.ColumnInt64(0);
    info.time = base::Time::FromInternalValue(statement.ColumnInt64(1));
    info.transition = content::PageTransitionFromInt(statement.ColumnInt(2));
    result_vector->push_back(info);
  }
}

}  // namespace history
