// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_

#include "base/macros.h"
#include "components/history/core/browser/history_types.h"

class HistoryService;

namespace history {

class HistoryServiceObserver {
 public:
  HistoryServiceObserver() {}
  virtual ~HistoryServiceObserver() {}

  // Called on changes to the VisitDatabase.
  virtual void OnAddVisit(HistoryService* history_service,
                          const BriefVisitInfo& info) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryServiceObserver);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_SERVICE_OBSERVER_H_
