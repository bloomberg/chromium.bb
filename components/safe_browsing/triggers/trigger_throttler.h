// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_THROTTLER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_THROTTLER_H_

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace safe_browsing {

enum class TriggerType {
  SECURITY_INTERSTITIAL = 1,
};

struct TriggerTypeHash {
  std::size_t operator()(TriggerType trigger_type) const {
    return static_cast<std::size_t>(trigger_type);
  };
};

// A map for storing a list of event timestamps for different trigger types.
using TriggerTimestampMap =
    std::unordered_map<TriggerType, std::vector<time_t>, TriggerTypeHash>;

// TriggerThrottler keeps track of how often each type of trigger gets fired
// and throttles them if they fire too often.
class TriggerThrottler {
 public:
  TriggerThrottler();
  virtual ~TriggerThrottler();

  // Check if the the specified |trigger_type| has quota available and is
  // allowed to fire at this time.
  bool TriggerCanFire(const TriggerType trigger_type) const;

  // Called to notify the throttler that a trigger has just fired and quota
  // should be updated.
  void TriggerFired(const TriggerType trigger_type);

 private:
  // Called to periodically clean-up the list of event timestamps.
  void CleanupOldEvents();

  // Stores each trigger type that fired along with the timestamps of when it
  // fired.
  TriggerTimestampMap trigger_events_;

  DISALLOW_COPY_AND_ASSIGN(TriggerThrottler);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_THROTTLER_H_
