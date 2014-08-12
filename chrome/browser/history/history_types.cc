// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_types.h"

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"

namespace history {

// VisitRow --------------------------------------------------------------------

VisitRow::VisitRow()
    : visit_id(0),
      url_id(0),
      referring_visit(0),
      transition(content::PAGE_TRANSITION_LINK),
      segment_id(0) {
}

VisitRow::VisitRow(URLID arg_url_id,
                   base::Time arg_visit_time,
                   VisitID arg_referring_visit,
                   content::PageTransition arg_transition,
                   SegmentID arg_segment_id)
    : visit_id(0),
      url_id(arg_url_id),
      visit_time(arg_visit_time),
      referring_visit(arg_referring_visit),
      transition(arg_transition),
      segment_id(arg_segment_id) {
}

VisitRow::~VisitRow() {
}

// QueryURLResult -------------------------------------------------------------

QueryURLResult::QueryURLResult() : success(false) {
}

QueryURLResult::~QueryURLResult() {
}

// HistoryAddPageArgs ---------------------------------------------------------

HistoryAddPageArgs::HistoryAddPageArgs()
    : context_id(NULL),
      page_id(0),
      transition(content::PAGE_TRANSITION_LINK),
      visit_source(SOURCE_BROWSED),
      did_replace_entry(false) {}

HistoryAddPageArgs::HistoryAddPageArgs(
    const GURL& url,
    base::Time time,
    ContextID context_id,
    int32 page_id,
    const GURL& referrer,
    const history::RedirectList& redirects,
    content::PageTransition transition,
    VisitSource source,
    bool did_replace_entry)
      : url(url),
        time(time),
        context_id(context_id),
        page_id(page_id),
        referrer(referrer),
        redirects(redirects),
        transition(transition),
        visit_source(source),
        did_replace_entry(did_replace_entry) {
}

HistoryAddPageArgs::~HistoryAddPageArgs() {}

// VisitDatabaseObserver -------------------------------------------------------

VisitDatabaseObserver::~VisitDatabaseObserver() {}

}  // namespace history
