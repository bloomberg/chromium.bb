// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_database.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/timer.h"
#include "base/stl_util.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/page_transition_types.h"
#include "sql/statement.h"

// Rows, in order, of the visit table.
#define HISTORY_VISIT_ROW_FIELDS \
  " id,url,visit_time,from_visit,transition,segment_id,is_indexed "

namespace history {

// Performs analysis of all local browsing data in the visits table to
// assess the feasibility of performing prerendering on that data.
// Will emulate prerendering based on a simple heuristic: On each
// pageview, pick most likely next page viewed in the next 5 minutes
// based on historical data; keep a maximum of 5 prerenders; evict
// prerenders older than 5 minutes or based on likelihood.  Will report
// back hypothetical prerender rate & accuracy via histograms.
// TODO(tburkard): Remove this before the branch for the next beta is cut.
class VisitDatabase::VisitAnalysis {
 public:
  explicit VisitAnalysis(VisitDatabase* visit_database)
      : visit_database_(visit_database),
        visits_fetched_(false) {
    emulated_prerender_managers_.push_back(
        new EmulatedPrerenderManager(this,
                                     1,
                                     VA_PRERENDER_STARTED_1,
                                     VA_PRERENDER_USED_1));
    emulated_prerender_managers_.push_back(
        new EmulatedPrerenderManager(this,
                                     3,
                                     VA_PRERENDER_STARTED_3,
                                     VA_PRERENDER_USED_3));
    emulated_prerender_managers_.push_back(
        new EmulatedPrerenderManager(this,
                                     5,
                                     VA_PRERENDER_STARTED_5,
                                     VA_PRERENDER_USED_5));
  }
  ~VisitAnalysis() {
    STLDeleteElements(&emulated_prerender_managers_);
  }
  void Init() {
    // Only schedule the callback if we are actually run inside a message
    // loop.
    if (MessageLoop::current() && !timer_.IsRunning()) {
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(kFetchDelayMs),
                   this,
                   &VisitDatabase::VisitAnalysis::FetchHistory);
    }
  }
  void AddVisit(VisitRow* visit) {
    visits_to_process_.push_back(*visit);
    MaybeProcessVisits();
  }

 private:
  enum VA_EVENTS {
    VA_VISIT = 10,
    VA_VISIT_EXCLUDE_BACK_FORWARD = 11,
    VA_VISIT_EXCLUDE_HOME_PAGE = 12,
    VA_VISIT_EXCLUDE_REDIRECT_CHAIN = 13,
    VA_PRERENDER_STARTED_1 = 14,
    VA_PRERENDER_USED_1 = 15,
    VA_PRERENDER_STARTED_3 = 16,
    VA_PRERENDER_USED_3 = 17,
    VA_PRERENDER_STARTED_5 = 18,
    VA_PRERENDER_USED_5 = 19,
    VA_EVENT_MAX = 20
  };
  bool IsBackForward(content::PageTransition transition) {
    return (transition & content::PAGE_TRANSITION_FORWARD_BACK) != 0;
  }
  bool IsHomePage(content::PageTransition transition) {
    return (transition & content::PAGE_TRANSITION_HOME_PAGE) != 0;
  }
  bool IsIntermediateRedirect(content::PageTransition transition) {
    return (transition & content::PAGE_TRANSITION_CHAIN_END) == 0;
  }
  bool ShouldExcludeTransitionForPrediction(
      content::PageTransition transition) {
    return IsBackForward(transition) || IsHomePage(transition) ||
        IsIntermediateRedirect(transition);
  }
  void RecordEvent(VA_EVENTS event, base::Time timestamp) {
    int64 ts = timestamp.ToInternalValue();
    if (time_event_bitmaps_.count(ts) < 1)
      time_event_bitmaps_[ts] = 0;
    int mask = (1 << event);
    VLOG(1) << "event " << ts << " " << event;
    if (time_event_bitmaps_[ts] & mask) {
      VLOG(1) << "duplicate event";
      return;
    }
    VLOG(1) << "recording event";
    time_event_bitmaps_[ts] |= mask;
    UMA_HISTOGRAM_ENUMERATION("Prerender.LocalVisitEvents",
                              event, VA_EVENT_MAX);
  }
  class EmulatedPrerenderManager {
   public:
    struct PrerenderData {
      base::Time started;
      URLID url;
      double priority;
    };
    EmulatedPrerenderManager(VisitAnalysis* visit_analysis, int num_prerenders,
                             VA_EVENTS prerender_event,
                             VA_EVENTS prerender_used_event)
        : visit_analysis_(visit_analysis),
          num_prerenders_(num_prerenders),
          prerender_event_(prerender_event),
          prerender_used_event_(prerender_used_event) {
      for (int i = 0; i < num_prerenders; i++) {
        PrerenderData prerender_data;
        prerender_data.started = base::Time();
        prerender_data.url = 0;
        prerender_data.priority = 0.0;
        prerenders_.push_back(prerender_data);
      }
    }
    void MaybeRecordPrerenderUsed(const VisitRow& visit) {
      base::TimeDelta max_age =
          base::TimeDelta::FromSeconds(kPrerenderExpirationSeconds);
      for (int i = 0; i < num_prerenders_; i++) {
        if (prerenders_[i].started >= visit.visit_time - max_age &&
            prerenders_[i].url == visit.url_id) {
          VLOG(1) << "PRERENDER USED " << num_prerenders_
                    << " " << visit.url_id;
          prerenders_[i].started = base::Time();
          visit_analysis_->RecordEvent(prerender_used_event_, visit.visit_time);
        }
      }
    }
    void MaybeAddPrerender(
        const PrerenderData& prerender,
        const VisitRow& visit) {
      if (MaybeAddPrerenderInternal(prerender)) {
        visit_analysis_->RecordEvent(prerender_event_, visit.visit_time);
      }
      VLOG(1) << "Maybe add prerender result: " << num_prerenders_;
      for (int i = 0; i < static_cast<int>(prerenders_.size()); i++) {
        if (prerenders_[i].started == base::Time())
          VLOG(1) << "---";
        else
          VLOG(1) << prerenders_[i].url << " " << prerenders_[i].priority;
      }
    }
   private:
    bool MaybeAddPrerenderInternal(const PrerenderData& prerender) {
      base::Time cutoff = prerender.started -
          base::TimeDelta::FromSeconds(kPrerenderExpirationSeconds);
      int earliest = 0;
      int weakest = 0;
      for (int i = 1; i < num_prerenders_; i++) {
        if (prerenders_[i].url == prerender.url) {
          if (prerenders_[i].priority < prerender.priority)
            prerenders_[i].priority = prerender.priority;
          return false;
        }
        if (prerenders_[i].started < prerenders_[earliest].started)
          earliest = i;
        if (prerenders_[i].priority < prerenders_[weakest].priority)
          weakest = i;
      }
      if (prerenders_[earliest].started < cutoff) {
        prerenders_[earliest] = prerender;
        return true;
      }
      if (prerenders_[weakest].priority < prerender.priority) {
        prerenders_[weakest] = prerender;
        return true;
      }
      return false;
    }
    VisitAnalysis* visit_analysis_;
    int num_prerenders_;
    std::vector<PrerenderData> prerenders_;
    VA_EVENTS prerender_event_;
    VA_EVENTS prerender_used_event_;
  };
  void ProcessVisit(const VisitRow& visit) {
    VLOG(1) << "processing visit to " << visit.url_id;
    if (IsBackForward(visit.transition)) {
      RecordEvent(VA_VISIT_EXCLUDE_BACK_FORWARD, visit.visit_time);
      return;
    }
    if (IsHomePage(visit.transition)) {
      RecordEvent(VA_VISIT_EXCLUDE_HOME_PAGE, visit.visit_time);
      return;
    }
    if (IsIntermediateRedirect(visit.transition)) {
      RecordEvent(VA_VISIT_EXCLUDE_REDIRECT_CHAIN, visit.visit_time);
      return;
    }
    RecordEvent(VA_VISIT, visit.visit_time);
    UMA_HISTOGRAM_ENUMERATION(
        "Prerender.LocalVisitCoreTransition",
        visit.transition & content::PAGE_TRANSITION_CORE_MASK,
        content::PAGE_TRANSITION_LAST_CORE + 1);
    for (int i = 0; i < static_cast<int>(emulated_prerender_managers_.size());
         i++) {
      emulated_prerender_managers_[i]->MaybeRecordPrerenderUsed(visit);
    }
    base::TimeDelta max_age =
        base::TimeDelta::FromSeconds(kPrerenderExpirationSeconds);
    base::TimeDelta min_age =
        base::TimeDelta::FromMilliseconds(kMinSpacingMilliseconds);
    std::set<URLID> currently_found;
    std::map<URLID, int> found;
    int num_occurrences = 0;
    base::Time last_started;
    URLID best_prerender = 0;
    int best_count = 0;
    for (int i = 0; i < static_cast<int>(visits_.size()); i++) {
      if (!ShouldExcludeTransitionForPrediction(visits_[i].transition)) {
        VLOG(1) << visits_[i].visit_time.ToInternalValue()/1000000 << " "
                  << visits_[i].url_id;
        if (visits_[i].url_id == visit.url_id) {
          VLOG(1) << "****";
          last_started = visits_[i].visit_time;
          num_occurrences++;
          currently_found.clear();
          continue;
        }
        if (last_started > visits_[i].visit_time - max_age &&
            last_started < visits_[i].visit_time - min_age) {
          currently_found.insert(visits_[i].url_id);
          VLOG(1) << "OK";
        }
      } else {
        VLOG(1) << i << " (skipping)";
      }
      if (i == static_cast<int>(visits_.size()) - 1 ||
          visits_[i+1].url_id == visit.url_id) {
        VLOG(1) << "#########";
        // output last window
        for (std::set<URLID>::iterator it = currently_found.begin();
             it != currently_found.end();
             ++it) {
          if (found.count(*it) < 1)
            found[*it] = 0;
          found[*it]++;
          if (found[*it] > best_count) {
            best_count = found[*it];
            best_prerender = *it;
          }
        }
        currently_found.clear();
      }
    }
    VLOG(1) << "best hast count " << best_count << "/" << num_occurrences
            << ", URL " << best_prerender;
    // Only add a prerender if the page was viewed at least twice, and
    // at least 10% of the time.
    if (num_occurrences > 0 && best_count > 1 &&
        best_count * 10 > num_occurrences) {
      EmulatedPrerenderManager::PrerenderData prerender;
      prerender.started = visit.visit_time;
      prerender.url = best_prerender;
      prerender.priority = static_cast<double>(best_count) /
          static_cast<double>(num_occurrences);
      VLOG(1) << "requesting prerender";
      for (int i = 0; i < static_cast<int>(emulated_prerender_managers_.size());
           i++) {
        emulated_prerender_managers_[i]->MaybeAddPrerender(prerender, visit);
      }
    }
  }
  void MaybeProcessVisits() {
    if (!visits_fetched_)
      return;
    for (int i = 0; i < static_cast<int>(visits_to_process_.size()); i++) {
      VisitRow visit = visits_to_process_[i];
      ProcessVisit(visit);
      visits_.push_back(visit);
    }
    visits_to_process_.clear();
    VLOG(1) << "done processing new visit.  new visit size = "
            << visits_.size();
  }
  void FetchHistory() {
    VLOG(1) << "VDB: fetching history";
    visit_database_->GetAllVisitsInRange(base::Time(), base::Time(),
                                         kMaxVisitRecords, &visits_);
    VLOG(1) << "READ visits " << visits_.size();
    if (logging::GetVlogVerbosity() >= 1) {
      for (int i = 0; i < static_cast<int>(visits_.size()); i++) {
        VLOG(1) << " Read visit: " << visits_[i].url_id;
      }
    }
    UMA_HISTOGRAM_COUNTS("Prerender.LocalVisitDatabaseSize", visits_.size());

    VLOG(1) << "--- all visits read.";
    visits_fetched_ = true;
    MaybeProcessVisits();
  }
  static const int kMaxVisitRecords = 100 * 1000;
  static const int kFetchDelayMs = 30 * 1000;
  static const int kPrerenderExpirationSeconds = 300;
  static const int kMinSpacingMilliseconds = 500;
  VisitDatabase* visit_database_;
  base::OneShotTimer<VisitAnalysis> timer_;
  VisitVector visits_;
  VisitVector visits_to_process_;
  bool visits_fetched_;
  std::map<int64, int> time_event_bitmaps_;
  std::vector<EmulatedPrerenderManager *> emulated_prerender_managers_;
};

VisitDatabase::VisitDatabase() : visit_analysis_(new VisitAnalysis(this)) {
}

VisitDatabase::~VisitDatabase() {
}

bool VisitDatabase::InitVisitTable() {
  visit_analysis_->Init();
  if (!GetDB().DoesTableExist("visits")) {
    if (!GetDB().Execute("CREATE TABLE visits("
        "id INTEGER PRIMARY KEY,"
        "url INTEGER NOT NULL," // key of the URL this corresponds to
        "visit_time INTEGER NOT NULL,"
        "from_visit INTEGER,"
        "transition INTEGER DEFAULT 0 NOT NULL,"
        "segment_id INTEGER,"
        // True when we have indexed data for this visit.
        "is_indexed BOOLEAN)"))
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
  visit_analysis_->AddVisit(visit);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO visits "
      "(url, visit_time, from_visit, transition, segment_id, is_indexed) "
      "VALUES (?,?,?,?,?,?)"));
  statement.BindInt64(0, visit->url_id);
  statement.BindInt64(1, visit->visit_time.ToInternalValue());
  statement.BindInt64(2, visit->referring_visit);
  statement.BindInt64(3, visit->transition);
  statement.BindInt64(4, visit->segment_id);
  statement.BindInt64(5, visit->is_indexed);

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
              << "url_id = " << visit->visit_id;
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
      "url=?,visit_time=?,from_visit=?,transition=?,segment_id=?,is_indexed=? "
      "WHERE id=?"));
  statement.BindInt64(0, visit.url_id);
  statement.BindInt64(1, visit.visit_time.ToInternalValue());
  statement.BindInt64(2, visit.referring_visit);
  statement.BindInt64(3, visit.transition);
  statement.BindInt64(4, visit.segment_id);
  statement.BindInt64(5, visit.is_indexed);
  statement.BindInt64(6, visit.visit_id);

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
                                            VisitVector* visits) {
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
    // Make sure the URL this visit corresponds to is unique.
    if (found_urls.find(visit.url_id) != found_urls.end())
      continue;
    found_urls.insert(visit.url_id);
    visits->push_back(visit);

    if (max_count > 0 && static_cast<int>(visits->size()) >= max_count)
      break;
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

}  // namespace history
