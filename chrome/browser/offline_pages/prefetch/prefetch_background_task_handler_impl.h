// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TickClock;
}

namespace net {
class BackoffEntry;
}

namespace offline_pages {

// A class living on the PrefetchService that is able to ask the Android
// background task system to schedule or cancel our task.  Also manages the
// backoff, so we can schedule tasks at the appropriate time based on the
// backoff value.
class PrefetchBackgroundTaskHandlerImpl : public PrefetchBackgroundTaskHandler {
 public:
  explicit PrefetchBackgroundTaskHandlerImpl(PrefService* profile);
  ~PrefetchBackgroundTaskHandlerImpl() override;

  // PrefetchBackgroundTaskHandler implementation.
  void CancelBackgroundTask() override;
  void EnsureTaskScheduled() override;

  // Backoff control.  These functions directly modify/read prefs.
  void Backoff() override;
  void ResetBackoff() override;
  int GetAdditionalBackoffSeconds() const override;

  // This is used to construct the backoff value.
  void SetTickClockForTesting(base::TickClock* clock);

 private:
  std::unique_ptr<net::BackoffEntry> GetCurrentBackoff() const;
  void UpdateBackoff(net::BackoffEntry* backoff);

  PrefService* prefs_;
  base::TickClock* clock_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBackgroundTaskHandlerImpl);
};

void RegisterPrefetchBackgroundTaskPrefs(PrefRegistrySimple* registry);

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_BACKGROUND_TASK_HANDLER_IMPL_H_
