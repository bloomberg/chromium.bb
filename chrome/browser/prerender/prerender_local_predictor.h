// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_LOCAL_PREDICTOR_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_LOCAL_PREDICTOR_H_
#pragma once

#include <vector>

#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/visit_database.h"
#include "googleurl/src/gurl.h"

class HistoryService;

namespace prerender {

class PrerenderManager;

// PrerenderLocalPredictor maintains local browsing history to make prerender
// predictions.
// At this point, the class is not actually creating prerenders, but just
// recording timing stats about the effect prerendering would have.
class PrerenderLocalPredictor : history::VisitDatabaseObserver {
 public:
  enum Event {
    EVENT_CONSTRUCTED = 0,
    EVENT_INIT_SCHEDULED = 1,
    EVENT_INIT_STARTED = 2,
    EVENT_INIT_FAILED_NO_HISTORY = 3,
    EVENT_INIT_SUCCEEDED = 4,
    EVENT_ADD_VISIT = 5,
    EVENT_ADD_VISIT_INITIALIZED = 6,
    EVENT_ADD_VISIT_PRERENDER_IDENTIFIED = 7,
    EVENT_ADD_VISIT_RELEVANT_TRANSITION = 8,
    EVENT_ADD_VISIT_IDENTIFIED_PRERENDER_CANDIDATE = 9,
    EVENT_ADD_VISIT_PRERENDERING = 10,
    EVENT_GOT_PRERENDER_URL = 11,
    EVENT_ERROR_NO_PRERENDER_URL_FOR_PLT = 12,
    EVENT_ADD_VISIT_PRERENDERING_EXTENDED = 13,
    EVENT_MAX_VALUE
  };

  // A PrerenderLocalPredictor is owned by the PrerenderManager specified
  // in the constructor.  It will be destoryed at the time its owning
  // PrerenderManager is destroyed.
  explicit PrerenderLocalPredictor(PrerenderManager* prerender_manager);
  virtual ~PrerenderLocalPredictor();

  // history::VisitDatabaseObserver implementation
  virtual void OnAddVisit(const history::BriefVisitInfo& info) OVERRIDE;

  void OnLookupURL(history::URLID url_id, const GURL& url);

  void OnGetInitialVisitHistory(
      scoped_ptr<std::vector<history::BriefVisitInfo> > visit_history);

  void OnPLTEventForURL(const GURL& url, base::TimeDelta page_load_time);

 private:
  struct PrerenderData;
  HistoryService* GetHistoryIfExists() const;
  void Init();
  bool IsPrerenderStillValid(PrerenderData* prerender) const;
  bool DoesPrerenderMatchPLTRecord(PrerenderData* prerender,
                                   const GURL& url,
                                   base::TimeDelta plt) const;
  void RecordEvent(Event event) const;

  PrerenderManager* prerender_manager_;
  base::OneShotTimer<PrerenderLocalPredictor> timer_;

  // Delay after which to initialize, to avoid putting to much load on the
  // database thread early on when Chrome is starting up.
  static const int kInitDelayMs = 5 * 1000;

  CancelableRequestConsumer history_db_consumer_;

  scoped_ptr<std::vector<history::BriefVisitInfo> > visit_history_;

  // We keep a reference to the HistoryService which we registered to
  // observe.  On destruction, we have to remove ourselves from that history
  // service.  We can't just grab the HistoryService from the profile, because
  // the profile may have already given up its reference to it.  Doing nothing
  // in this case may cause crashes, because the HistoryService may outlive the
  // the PrerenderLocalPredictor.
  scoped_refptr<HistoryService> observing_history_service_;

  scoped_ptr<PrerenderData> current_prerender_;
  scoped_ptr<PrerenderData> last_swapped_in_prerender_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderLocalPredictor);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_LOCAL_PREDICTOR_H_
