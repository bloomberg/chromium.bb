// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visitsegment_database.h"

#include <math.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/chrome_switches.h"
#include "sql/statement.h"
#include "sql/transaction.h"

// The following tables are used to store url segment information.
//
// segments
//   id                 Primary key
//   name               A unique string to represent that segment. (URL derived)
//   url_id             ID of the url currently used to represent this segment.
//
// segment_usage
//   id                 Primary key
//   segment_id         Corresponding segment id
//   time_slot          time stamp identifying for what day this entry is about
//   visit_count        Number of visit in the segment
//
// segment_duration
//   id                 Primary key
//   segment_id         Corresponding segment id
//   time_slot          time stamp identifying what day this entry is for
//   duration           Total time during the time_slot the user has been on
//                      the page. This is a serialized TimeDelta value.
// segment_duration is only created if chrome::kTrackActiveVisitTime is set.

namespace history {

VisitSegmentDatabase::VisitSegmentDatabase()
    : has_duration_table_(CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kTrackActiveVisitTime)) {
}

VisitSegmentDatabase::~VisitSegmentDatabase() {
}

bool VisitSegmentDatabase::InitSegmentTables() {
  // Segments table.
  if (!GetDB().DoesTableExist("segments")) {
    if (!GetDB().Execute("CREATE TABLE segments ("
        "id INTEGER PRIMARY KEY,"
        "name VARCHAR,"
        "url_id INTEGER NON NULL)")) {
      return false;
    }

    if (!GetDB().Execute(
        "CREATE INDEX segments_name ON segments(name)")) {
      return false;
    }
  }

  // This was added later, so we need to try to create it even if the table
  // already exists.
  if (!GetDB().Execute("CREATE INDEX IF NOT EXISTS segments_url_id ON "
                       "segments(url_id)"))
    return false;

  // Segment usage table.
  if (!GetDB().DoesTableExist("segment_usage")) {
    if (!GetDB().Execute("CREATE TABLE segment_usage ("
        "id INTEGER PRIMARY KEY,"
        "segment_id INTEGER NOT NULL,"
        "time_slot INTEGER NOT NULL,"
        "visit_count INTEGER DEFAULT 0 NOT NULL)")) {
      return false;
    }
    if (!GetDB().Execute(
        "CREATE INDEX segment_usage_time_slot_segment_id ON "
        "segment_usage(time_slot, segment_id)")) {
      return false;
    }
  }

  // Added in a later version, so we always need to try to creat this index.
  if (!GetDB().Execute("CREATE INDEX IF NOT EXISTS segments_usage_seg_id "
                       "ON segment_usage(segment_id)"))
    return false;

  // TODO(sky): if we decide to keep this feature duration should be added to
  // segument_usage.
  if (has_duration_table_ && !GetDB().DoesTableExist("segment_duration")) {
    if (!GetDB().Execute("CREATE TABLE segment_duration ("
                         "id INTEGER PRIMARY KEY,"
                         "segment_id INTEGER NOT NULL,"
                         "time_slot INTEGER NOT NULL,"
                         "duration INTEGER DEFAULT 0 NOT NULL)")) {
      return false;
    }
    if (!GetDB().Execute(
            "CREATE INDEX segment_duration_time_slot_segment_id ON "
            "segment_duration(time_slot, segment_id)")) {
      return false;
    }
  } else if (!has_duration_table_ &&
             !GetDB().Execute("DROP TABLE IF EXISTS segment_duration")) {
    return false;
  }

  return true;
}

bool VisitSegmentDatabase::DropSegmentTables() {
  // Dropping the tables will implicitly delete the indices.
  return GetDB().Execute("DROP TABLE segments") &&
         GetDB().Execute("DROP TABLE segment_usage") &&
         GetDB().Execute("DROP TABLE IF EXISTS segment_duration");
}

// Note: the segment name is derived from the URL but is not a URL. It is
// a string that can be easily recreated from various URLS. Maybe this should
// be an MD5 to limit the length.
//
// static
std::string VisitSegmentDatabase::ComputeSegmentName(const GURL& url) {
  // TODO(brettw) this should probably use the registry controlled
  // domains service.
  GURL::Replacements r;
  const char kWWWDot[] = "www.";
  const int kWWWDotLen = arraysize(kWWWDot) - 1;

  std::string host = url.host();
  const char* host_c = host.c_str();
  // Remove www. to avoid some dups.
  if (static_cast<int>(host.size()) > kWWWDotLen &&
      LowerCaseEqualsASCII(host_c, host_c + kWWWDotLen, kWWWDot)) {
    r.SetHost(host.c_str(),
              url_parse::Component(kWWWDotLen,
                  static_cast<int>(host.size()) - kWWWDotLen));
  }
  // Remove other stuff we don't want.
  r.ClearUsername();
  r.ClearPassword();
  r.ClearQuery();
  r.ClearRef();
  r.ClearPort();

  return url.ReplaceComponents(r).spec();
}

// static
base::Time VisitSegmentDatabase::SegmentTime(base::Time time) {
  return time.LocalMidnight();
}

SegmentID VisitSegmentDatabase::GetSegmentNamed(
    const std::string& segment_name) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM segments WHERE name = ?"));
  statement.BindString(0, segment_name);

  if (statement.Step())
    return statement.ColumnInt64(0);
  return 0;
}

bool VisitSegmentDatabase::UpdateSegmentRepresentationURL(SegmentID segment_id,
                                                          URLID url_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE segments SET url_id = ? WHERE id = ?"));
  statement.BindInt64(0, url_id);
  statement.BindInt64(1, segment_id);

  return statement.Run();
}

URLID VisitSegmentDatabase::GetSegmentRepresentationURL(SegmentID segment_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT url_id FROM segments WHERE id = ?"));
  statement.BindInt64(0, segment_id);

  if (statement.Step())
    return statement.ColumnInt64(0);
  return 0;
}

SegmentID VisitSegmentDatabase::CreateSegment(URLID url_id,
                                              const std::string& segment_name) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO segments (name, url_id) VALUES (?,?)"));
  statement.BindString(0, segment_name);
  statement.BindInt64(1, url_id);

  if (statement.Run())
    return GetDB().GetLastInsertRowId();
  return 0;
}

bool VisitSegmentDatabase::IncreaseSegmentVisitCount(SegmentID segment_id,
                                                     base::Time ts,
                                                     int amount) {
  base::Time t = SegmentTime(ts);

  sql::Statement select(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, visit_count FROM segment_usage "
      "WHERE time_slot = ? AND segment_id = ?"));
  select.BindInt64(0, t.ToInternalValue());
  select.BindInt64(1, segment_id);

  if (!select.is_valid())
    return false;

  if (select.Step()) {
    sql::Statement update(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "UPDATE segment_usage SET visit_count = ? WHERE id = ?"));
    update.BindInt64(0, select.ColumnInt64(1) + static_cast<int64>(amount));
    update.BindInt64(1, select.ColumnInt64(0));

    return update.Run();
  } else {
    sql::Statement insert(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "INSERT INTO segment_usage "
        "(segment_id, time_slot, visit_count) VALUES (?, ?, ?)"));
    insert.BindInt64(0, segment_id);
    insert.BindInt64(1, t.ToInternalValue());
    insert.BindInt64(2, static_cast<int64>(amount));

    return insert.Run();
  }
}

void VisitSegmentDatabase::QuerySegmentUsage(
    base::Time from_time,
    int max_result_count,
    std::vector<PageUsageData*>* result) {
  // Gather all the segment scores.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT segment_id, time_slot, visit_count "
      "FROM segment_usage WHERE time_slot >= ? "
      "ORDER BY segment_id"));
  if (!statement.is_valid())
    return;

  QuerySegmentsCommon(&statement, from_time, max_result_count,
                      QUERY_VISIT_COUNT, result);
}

bool VisitSegmentDatabase::DeleteSegmentData(base::Time older_than) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segment_usage WHERE time_slot < ?"));
  statement.BindInt64(0, SegmentTime(older_than).ToInternalValue());

  if (!statement.Run())
    return false;

  if (!has_duration_table_)
    return true;

  sql::Statement duration_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segment_duration WHERE time_slot < ?"));
  duration_statement.BindInt64(0, SegmentTime(older_than).ToInternalValue());

  return duration_statement.Run();
}

bool VisitSegmentDatabase::DeleteSegmentForURL(URLID url_id) {
  sql::Statement delete_usage(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segment_usage WHERE segment_id IN "
      "(SELECT id FROM segments WHERE url_id = ?)"));
  delete_usage.BindInt64(0, url_id);

  if (!delete_usage.Run())
    return false;

  if (has_duration_table_) {
    sql::Statement delete_duration(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "DELETE FROM segment_duration WHERE segment_id IN "
        "(SELECT id FROM segments WHERE url_id = ?)"));
    delete_duration.BindInt64(0, url_id);

    if (!delete_duration.Run())
      return false;
  }

  sql::Statement delete_seg(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segments WHERE url_id = ?"));
  delete_seg.BindInt64(0, url_id);

  return delete_seg.Run();
}

SegmentDurationID VisitSegmentDatabase::CreateSegmentDuration(
    SegmentID segment_id,
    base::Time time,
    base::TimeDelta delta) {
  if (!has_duration_table_)
    return 0;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO segment_duration (segment_id, time_slot, duration) "
      "VALUES (?,?,?)"));
  statement.BindInt64(0, segment_id);
  statement.BindInt64(1, SegmentTime(time).ToInternalValue());
  statement.BindInt64(2, delta.ToInternalValue());
  return statement.Run() ? GetDB().GetLastInsertRowId() : 0;
}

bool VisitSegmentDatabase::SetSegmentDuration(SegmentDurationID duration_id,
                                              base::TimeDelta time_delta) {
  if (!has_duration_table_)
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE segment_duration SET duration = ? WHERE id = ?"));
  statement.BindInt64(0, time_delta.ToInternalValue());
  statement.BindInt64(1, duration_id);
  return statement.Run();
}

bool VisitSegmentDatabase::GetSegmentDuration(SegmentID segment_id,
                                              base::Time time,
                                              SegmentDurationID* duration_id,
                                              base::TimeDelta* time_delta) {
  if (!has_duration_table_)
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, duration FROM segment_duration "
      "WHERE segment_id = ? AND time_slot = ? "));
  if (!statement.is_valid())
    return false;

  statement.BindInt64(0, segment_id);
  statement.BindInt64(1, SegmentTime(time).ToInternalValue());

  if (!statement.Step())
    return false;

  *duration_id = statement.ColumnInt64(0);
  *time_delta = base::TimeDelta::FromInternalValue(statement.ColumnInt64(1));
  return true;
}

void VisitSegmentDatabase::QuerySegmentDuration(
    base::Time from_time,
    int max_result_count,
    std::vector<PageUsageData*>* result) {
  if (!has_duration_table_)
    return;

  // Gather all the segment scores.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT segment_id, time_slot, duration "
      "FROM segment_duration WHERE time_slot >= ? "
      "ORDER BY segment_id"));
  if (!statement.is_valid())
    return;

  QuerySegmentsCommon(&statement, from_time, max_result_count, QUERY_DURATION,
                      result);
}

bool VisitSegmentDatabase::MigratePresentationIndex() {
  sql::Transaction transaction(&GetDB());
  return transaction.Begin() &&
      GetDB().Execute("DROP TABLE presentation") &&
      GetDB().Execute("CREATE TABLE segments_tmp ("
                      "id INTEGER PRIMARY KEY,"
                      "name VARCHAR,"
                      "url_id INTEGER NON NULL)") &&
      GetDB().Execute("INSERT INTO segments_tmp SELECT "
                      "id, name, url_id FROM segments") &&
      GetDB().Execute("DROP TABLE segments") &&
      GetDB().Execute("ALTER TABLE segments_tmp RENAME TO segments") &&
      transaction.Commit();
}


void VisitSegmentDatabase::QuerySegmentsCommon(
    sql::Statement* statement,
    base::Time from_time,
    int max_result_count,
    QueryType query_type,
    std::vector<PageUsageData*>* result) {
  // This function gathers the highest-ranked segments in two queries.
  // The first gathers scores for all segments.
  // The second gathers segment data (url, title, etc.) for the highest-ranked
  // segments.

  base::Time ts = SegmentTime(from_time);
  statement->BindInt64(0, ts.ToInternalValue());

  base::Time now = base::Time::Now();
  SegmentID last_segment_id = 0;
  PageUsageData* pud = NULL;
  float score = 0;
  base::TimeDelta duration;
  while (statement->Step()) {
    SegmentID segment_id = statement->ColumnInt64(0);
    if (segment_id != last_segment_id) {
      if (pud) {
        pud->SetScore(score);
        pud->SetDuration(duration);
        result->push_back(pud);
      }

      pud = new PageUsageData(segment_id);
      score = 0;
      last_segment_id = segment_id;
      duration = base::TimeDelta();
    }

    base::Time timeslot =
        base::Time::FromInternalValue(statement->ColumnInt64(1));
    int count;
    if (query_type == QUERY_VISIT_COUNT) {
      count = statement->ColumnInt(2);
    } else {
      base::TimeDelta current_duration(
          base::TimeDelta::FromInternalValue(statement->ColumnInt64(2)));
      duration += current_duration;
      // Souldn't overflow since we group by day.
      count = static_cast<int>(current_duration.InSeconds());
    }
    float day_score = 1.0f + log(static_cast<float>(count));

    // Recent visits count more than historical ones, so we multiply in a boost
    // related to how long ago this day was.
    // This boost is a curve that smoothly goes through these values:
    // Today gets 3x, a week ago 2x, three weeks ago 1.5x, falling off to 1x
    // at the limit of how far we reach into the past.
    int days_ago = (now - timeslot).InDays();
    float recency_boost = 1.0f + (2.0f * (1.0f / (1.0f + days_ago/7.0f)));
    score += recency_boost * day_score;
  }

  if (pud) {
    pud->SetScore(score);
    pud->SetDuration(duration);
    result->push_back(pud);
  }

  // Limit to the top kResultCount results.
  std::sort(result->begin(), result->end(), PageUsageData::Predicate);
  if (static_cast<int>(result->size()) > max_result_count) {
    STLDeleteContainerPointers(result->begin() + max_result_count,
                               result->end());
    result->resize(max_result_count);
  }

  // Now fetch the details about the entries we care about.
  sql::Statement statement2(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT urls.url, urls.title FROM urls "
      "JOIN segments ON segments.url_id = urls.id "
      "WHERE segments.id = ?"));

  if (!statement2.is_valid())
    return;

  for (size_t i = 0; i < result->size(); ++i) {
    PageUsageData* pud = (*result)[i];
    statement2.BindInt64(0, pud->GetID());
    if (statement2.Step()) {
      pud->SetURL(GURL(statement2.ColumnString(0)));
      pud->SetTitle(statement2.ColumnString16(1));
    }
    statement2.Reset(true);
  }
}

}  // namespace history
