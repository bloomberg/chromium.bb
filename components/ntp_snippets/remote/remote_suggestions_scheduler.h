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
  // Internal triggers to consider fetching content suggestions.

  // Called whenever the remote suggestions provider becomes active (on startup,
  // or later on).
  virtual void OnProviderActivated() = 0;

  // Called whenever the remote suggestions provider becomes inactive (on
  // startup, or later on).
  virtual void OnProviderDeactivated() = 0;

  // Called whenever the remote suggestions provider clears all suggestions.
  virtual void OnSuggestionsCleared() = 0;

  // Called whenever the remote suggestions provider clears all suggestions
  // because history gets cleared (and we must not show them any more).
  virtual void OnHistoryCleared() = 0;

  // External triggers to consider fetching content suggestions.

  // Called whenever chrome is started warm or the user switches to Chrome.
  virtual void OnBrowserForegrounded() = 0;

  // Called whenever chrome is cold started.
  // To keep start ups fast, defer any work possible.
  virtual void OnBrowserColdStart() = 0;

  // Called whenever a new NTP is opened. This may be called on cold starts.
  // So to keep start ups fast, defer heavy work for cold starts.
  virtual void OnNTPOpened() = 0;

  // Fetch content suggestions.
  virtual void OnPersistentSchedulerWakeUp() = 0;

  // Force rescheduling of fetching.
  virtual void RescheduleFetching() = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_
