// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_SCHEDULER_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace ntp_snippets {

// Class to take care of scheduling of periodic updates of snippets. There are
// two types of scheduled updates:
//  - "hard" ones that should outlive current running instance of Chrome. These
//  should get triggered according to their schedule even if Chrome is not
//  running at the given moment. This is OS-dependent, may be unavilable on
//  some platforms.
//  - "soft" ones that get triggered only if Chrome stays running until the
//  scheduled point.
class RemoteSuggestionsScheduler {
 public:
  // Interface to perform the scheduled update.
  class Updater {
    virtual void HardUpdate();
    virtual void SoftUpdate();
  };

  // The passed in |updater| is called when an update is due according to the
  // schedule. Note that hard fetches get access to the |updater| via the keyed
  // ContentSuggestionService because the concrete instance passed to
  // RemoteSuggestionsScheduler when the hard fetch was scheduled may not exist
  // any more when the hard update is due.
  explicit RemoteSuggestionsScheduler(Updater* updater);

  // Schedules both "soft" and "hard" fetches. First removes existing schedule
  // before scheduling new updates.
  void Schedule();

  // Removes any existing schedule.
  void Unschedule();

  // Schedule periodic fetching of snippets, with different periods depending on
  // network state. Once per period, the concrete implementation should call
  // RemoteSuggestionsUpdater::HardUpdate where RemoteSuggestionsUpdater is
  // obtained from ContentSuggestionsService.
  // Any of the periods can be zero to indicate that the corresponding task
  // should not be scheduled.
  virtual bool Schedule(base::TimeDelta period_wifi,
                        base::TimeDelta period_fallback) = 0;

  // Cancel any scheduled tasks.
  virtual bool Unschedule() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsHardScheduler);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_HARD_SCHEDULER_H_
