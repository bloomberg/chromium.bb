// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HARD_SCHEDULER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HARD_SCHEDULER_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace ntp_snippets {

// Interface to schedule "hard" periodic fetches of snippets. These "hard"
// fetches must get triggered according to their schedule independent of whether
// Chrome is running at that moment.
class RemoteSuggestionsHardScheduler {
 public:
  // Schedule periodic fetching of snippets, with different periods depending on
  // network state. Once per period, the concrete implementation should call
  // RemoteSuggestionsScheduler::Updater::HardUpdate() where the updater is
  // obtained from ContentSuggestionsService. Any of the periods can be zero to
  // indicate that the corresponding task should not be scheduled.
  virtual bool Schedule(base::TimeDelta period_wifi,
                        base::TimeDelta period_fallback) = 0;

  // Cancel any scheduled tasks.
  virtual bool Unschedule() = 0;

 protected:
  RemoteSuggestionsHardScheduler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsHardScheduler);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HARD_SCHEDULER_H_
