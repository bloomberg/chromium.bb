// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_
#define CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class CHROMEOS_EXPORT PortalDetectorStrategy {
 public:
  enum StrategyId {
    STRATEGY_ID_LOGIN_SCREEN,
    STRATEGY_ID_ERROR_SCREEN,
    STRATEGY_ID_SESSION
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns number of performed attempts.
    virtual int AttemptCount() = 0;

    // Returns time when current attempt was started.
    virtual base::TimeTicks AttemptStartTime() = 0;

    // Returns current TimeTicks.
    virtual base::TimeTicks GetCurrentTimeTicks() = 0;
  };

  virtual ~PortalDetectorStrategy();

  static scoped_ptr<PortalDetectorStrategy> CreateById(StrategyId id);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Returns true when detection attempt can be performed according to
  // current strategy.
  bool CanPerformAttempt();

  // Returns true if additional attempt could be scheduled after
  // detection.
  bool CanPerformAttemptAfterDetection();

  // Returns delay before next detection attempt. This delay is needed
  // to separate detection attempts in time.
  base::TimeDelta GetDelayTillNextAttempt();

  // Returns timeout for the next detection attempt.
  base::TimeDelta GetNextAttemptTimeout();

  virtual StrategyId Id() const = 0;

 protected:
  PortalDetectorStrategy();

  // Interface for subclasses:
  virtual bool CanPerformAttemptImpl();
  virtual bool CanPerformAttemptAfterDetectionImpl();
  virtual base::TimeDelta GetDelayTillNextAttemptImpl();
  virtual base::TimeDelta GetNextAttemptTimeoutImpl();

  // Adjusts |delay| according to current attempt count and elapsed time
  // since previous attempt.
  base::TimeDelta AdjustDelay(const base::TimeDelta& delay);

  Delegate* delegate_;

 private:
  friend class NetworkPortalDetectorImplTest;
  friend class NetworkPortalDetectorImplBrowserTest;

  static void set_delay_till_next_attempt_for_testing(
      const base::TimeDelta& timeout) {
    delay_till_next_attempt_for_testing_ = timeout;
    delay_till_next_attempt_for_testing_initialized_ = true;
  }

  static void set_next_attempt_timeout_for_testing(
      const base::TimeDelta& timeout) {
    next_attempt_timeout_for_testing_ = timeout;
    next_attempt_timeout_for_testing_initialized_ = true;
  }

  static void reset_fields_for_testing() {
    delay_till_next_attempt_for_testing_initialized_ = false;
    next_attempt_timeout_for_testing_initialized_ = false;
  }

  // Test delay before detection attempt, used by unit tests.
  static base::TimeDelta delay_till_next_attempt_for_testing_;

  // True when |min_time_between_attempts_for_testing_| is initialized.
  static bool delay_till_next_attempt_for_testing_initialized_;

  // Test timeout for a detection attempt, used by unit tests.
  static base::TimeDelta next_attempt_timeout_for_testing_;

  // True when |next_attempt_timeout_for_testing_| is initialized.
  static bool next_attempt_timeout_for_testing_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PortalDetectorStrategy);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_
