// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_

#include "components/history/core/browser/history_types.h"
#include "content/public/common/page_transition_types.h"

namespace content {
class WebContents;
}

namespace history {

// Identifier for a context to scope page ids. (ContextIDs are used in
// comparisons only and are never dereferenced.)
// NB: The use of WebContents here is temporary; when the dependency on content
// is broken, some other type will take its place.
typedef content::WebContents* ContextID;

// VisitRow -------------------------------------------------------------------

// Holds all information associated with a specific visit. A visit holds time
// and referrer information for one time a URL is visited.
class VisitRow {
 public:
  VisitRow();
  VisitRow(URLID arg_url_id,
           base::Time arg_visit_time,
           VisitID arg_referring_visit,
           content::PageTransition arg_transition,
           SegmentID arg_segment_id);
  ~VisitRow();

  // ID of this row (visit ID, used a a referrer for other visits).
  VisitID visit_id;

  // Row ID into the URL table of the URL that this page is.
  URLID url_id;

  base::Time visit_time;

  // Indicates another visit that was the referring page for this one.
  // 0 indicates no referrer.
  VisitID referring_visit;

  // A combination of bits from PageTransition.
  content::PageTransition transition;

  // The segment id (see visitsegment_database.*).
  // If 0, the segment id is null in the table.
  SegmentID segment_id;

  // Record how much time a user has this visit starting from the user
  // opened this visit to the user closed or ended this visit.
  // This includes both active and inactive time as long as
  // the visit was present.
  base::TimeDelta visit_duration;

  // Compares two visits based on dates, for sorting.
  bool operator<(const VisitRow& other) {
    return visit_time < other.visit_time;
  }

  // We allow the implicit copy constuctor and operator=.
};

// We pass around vectors of visits a lot
typedef std::vector<VisitRow> VisitVector;

// The basic information associated with a visit (timestamp, type of visit),
// used by HistoryBackend::AddVisits() to create new visits for a URL.
typedef std::pair<base::Time, content::PageTransition> VisitInfo;

// QueryURLResult -------------------------------------------------------------

// QueryURLResult encapsulates the result of a call to HistoryBackend::QueryURL.
struct QueryURLResult {
  QueryURLResult();
  ~QueryURLResult();

  // Indicates whether the call to HistoryBackend::QueryURL was successfull
  // or not. If false, then both |row| and |visits| fields are undefined.
  bool success;
  URLRow row;
  VisitVector visits;
};

// Navigation -----------------------------------------------------------------

// Marshalling structure for AddPage.
struct HistoryAddPageArgs {
  // The default constructor is equivalent to:
  //
  //   HistoryAddPageArgs(
  //       GURL(), base::Time(), NULL, 0, GURL(),
  //       history::RedirectList(), content::PAGE_TRANSITION_LINK,
  //       SOURCE_BROWSED, false)
  HistoryAddPageArgs();
  HistoryAddPageArgs(const GURL& url,
                     base::Time time,
                     ContextID context_id,
                     int32 page_id,
                     const GURL& referrer,
                     const history::RedirectList& redirects,
                     content::PageTransition transition,
                     VisitSource source,
                     bool did_replace_entry);
  ~HistoryAddPageArgs();

  GURL url;
  base::Time time;

  ContextID context_id;
  int32 page_id;

  GURL referrer;
  history::RedirectList redirects;
  content::PageTransition transition;
  VisitSource visit_source;
  bool did_replace_entry;
};

// Abbreviated information about a visit.
struct BriefVisitInfo {
  URLID url_id;
  base::Time time;
  content::PageTransition transition;
};

// An observer of VisitDatabase.
class VisitDatabaseObserver {
 public:
  virtual ~VisitDatabaseObserver();
  virtual void OnAddVisit(const BriefVisitInfo& info) = 0;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TYPES_H_
