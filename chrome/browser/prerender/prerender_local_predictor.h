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
// At this point, the class is just illustrating the interface with the
// Chrome History.
// TODO(tburkard): Fill in actual prerender prediction logic.
class PrerenderLocalPredictor : history::VisitDatabaseObserver {
 public:
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

 private:
  HistoryService* GetHistoryIfExists() const;
  void Init();

  PrerenderManager* prerender_manager_;
  base::OneShotTimer<PrerenderLocalPredictor> timer_;

  // Delay after which to initialize, to avoid putting to much load on the
  // database thread early on when Chrome is starting up.
  static const int kInitDelayMs = 5 * 1000;

  CancelableRequestConsumer history_db_consumer_;

  scoped_ptr<std::vector<history::BriefVisitInfo> > visit_history_;
  bool visit_history_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderLocalPredictor);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_LOCAL_PREDICTOR_H_
