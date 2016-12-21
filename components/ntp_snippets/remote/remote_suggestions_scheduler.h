// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_

#include "base/macros.h"

namespace ntp_snippets {

// Interface for informing the scheduler.
class RemoteSuggestionsScheduler {
 public:
  // Fetch content suggestions.
  virtual void OnPersistentSchedulerWakeUp() = 0;

  // Force rescheduling of fetching.
  virtual void RescheduleFetching() = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_
