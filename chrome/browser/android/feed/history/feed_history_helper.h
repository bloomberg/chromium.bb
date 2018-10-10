// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_HISTORY_FEED_HISTORY_HELPER_H_
#define CHROME_BROWSER_ANDROID_FEED_HISTORY_FEED_HISTORY_HELPER_H_

#include "base/task/cancelable_task_tracker.h"
#include "components/feed/core/feed_logging_metrics.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history {
class HistoryService;
class URLRow;
}  // namespace history

namespace feed {

// This class is help feed components to check history service.
class FeedHistoryHelper {
 public:
  explicit FeedHistoryHelper(history::HistoryService* history_service);
  ~FeedHistoryHelper();

  void CheckURL(GURL url, FeedLoggingMetrics::CheckURLVisitCallback callback);

  base::WeakPtr<FeedHistoryHelper> AsWeakPtr();

 private:
  history::HistoryService* history_service_;
  base::CancelableTaskTracker tracker_;

  void OnCheckURLDone(FeedLoggingMetrics::CheckURLVisitCallback callback,
                      bool success,
                      const history::URLRow& row,
                      const history::VisitVector& visit_vector);

  base::WeakPtrFactory<FeedHistoryHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedHistoryHelper);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_HISTORY_FEED_HISTORY_HELPER_H_
