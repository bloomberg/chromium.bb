// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace history {
class HistoryService;
}  // namespace history

namespace url {
class Origin;
}  // namespace url

// Lives entirely on the UI thread.
class BackgroundSyncMetrics {
 public:
  explicit BackgroundSyncMetrics(history::HistoryService* history_service);
  ~BackgroundSyncMetrics();

  void MaybeRecordRegistrationEvent(const url::Origin& origin,
                                    bool can_fire,
                                    bool is_reregistered);

  void MaybeRecordCompletionEvent(const url::Origin& origin,
                                  blink::ServiceWorkerStatusCode status_code,
                                  int num_attempts,
                                  int max_attempts);

 private:
  friend class BackgroundSyncMetricsBrowserTest;

  void DidGetVisibleVisitCount(base::OnceClosure visit_closure,
                               bool did_determine,
                               int num_visits,
                               base::Time first_visit_time);
  void RecordRegistrationEvent(const url::Origin& origin,
                               bool can_fire,
                               bool is_reregistered);

  void RecordCompletionEvent(const url::Origin& origin,
                             blink::ServiceWorkerStatusCode status_code,
                             int num_attempts,
                             int max_attempts);

  history::HistoryService* history_service_;

  // Task tracker used for querying URLs in the history service.
  base::CancelableTaskTracker task_tracker_;

  // Used to signal tests that a UKM event has been recorded.
  base::OnceClosure ukm_event_recorded_for_testing_;

  base::WeakPtrFactory<BackgroundSyncMetrics> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncMetrics);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_
