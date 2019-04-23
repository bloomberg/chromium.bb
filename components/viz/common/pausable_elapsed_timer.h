// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_PAUSABLE_ELAPSED_TIMER_H_
#define COMPONENTS_VIZ_COMMON_PAUSABLE_ELAPSED_TIMER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// A extension of base::ElapsedTimer to support pausing. The timer starts off in
// a paused state. This allows the calculation of the duration of a task to not
// include the time when it is not running.
class VIZ_COMMON_EXPORT PausableElapsedTimer {
 public:
  PausableElapsedTimer();

  // Returns the time elapsed since object construction excluding the time when
  // the timer is paused.
  base::TimeDelta Elapsed() const;

  // Pause the timer. No-op if already paused.
  void Pause();

  // Resume the timer. No-op is not currently paused.
  void Resume();

 private:
  base::Optional<base::ElapsedTimer> active_timer_;
  base::TimeDelta previous_elapsed_;

  DISALLOW_COPY_AND_ASSIGN(PausableElapsedTimer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_PAUSABLE_ELAPSED_TIMER_H_
