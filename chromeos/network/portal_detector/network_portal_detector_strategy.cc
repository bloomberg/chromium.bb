// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"

#include "base/logging.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

namespace {

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

// TODO (ygorshenin@): reuse net::BackoffEntry for strategies.

class LoginScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kMaxAttempts = 3;
  static const int kDelayBetweenAttemptsSec = 3;
  static const int kBaseAttemptTimeoutSec = 5;

  LoginScreenStrategy() {}
  virtual ~LoginScreenStrategy() {}

 protected:
  // PortalDetectorStrategy overrides:
  virtual StrategyId Id() const OVERRIDE { return STRATEGY_ID_LOGIN_SCREEN; }
  virtual bool CanPerformAttemptImpl() OVERRIDE {
    return delegate_->AttemptCount() < kMaxAttempts;
  }
  virtual base::TimeDelta GetDelayTillNextAttemptImpl() OVERRIDE {
    return AdjustDelay(base::TimeDelta::FromSeconds(kDelayBetweenAttemptsSec));
  }
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() OVERRIDE {
    int timeout = DefaultNetwork()
                      ? (delegate_->AttemptCount() + 1) * kBaseAttemptTimeoutSec
                      : kBaseAttemptTimeoutSec;
    return base::TimeDelta::FromSeconds(timeout);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenStrategy);
};

class ErrorScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kDelayBetweenAttemptsSec = 3;
  static const int kAttemptTimeoutSec = 15;

  ErrorScreenStrategy() {}
  virtual ~ErrorScreenStrategy() {}

 protected:
  // PortalDetectorStrategy overrides:
  virtual StrategyId Id() const OVERRIDE { return STRATEGY_ID_ERROR_SCREEN; }
  virtual bool CanPerformAttemptImpl() OVERRIDE { return true; }
  virtual bool CanPerformAttemptAfterDetectionImpl() OVERRIDE { return true; }
  virtual base::TimeDelta GetDelayTillNextAttemptImpl() OVERRIDE {
    return AdjustDelay(base::TimeDelta::FromSeconds(kDelayBetweenAttemptsSec));
  }
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() OVERRIDE {
    return base::TimeDelta::FromSeconds(kAttemptTimeoutSec);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ErrorScreenStrategy);
};

class SessionStrategy : public PortalDetectorStrategy {
 public:
  static const int kFastDelayBetweenAttemptsSec = 1;
  static const int kFastAttemptTimeoutSec = 3;
  static const int kMaxFastAttempts = 3;

  static const int kNormalDelayBetweenAttemptsSec = 10;
  static const int kNormalAttemptTimeoutSec = 5;
  static const int kMaxNormalAttempts = 3;

  static const int kSlowDelayBetweenAttemptsSec = 2 * 60;
  static const int kSlowAttemptTimeoutSec = 5;

  SessionStrategy() {}
  virtual ~SessionStrategy() {}

 protected:
  virtual StrategyId Id() const OVERRIDE { return STRATEGY_ID_SESSION; }
  virtual bool CanPerformAttemptImpl() OVERRIDE { return true; }
  virtual bool CanPerformAttemptAfterDetectionImpl() OVERRIDE { return true; }
  virtual base::TimeDelta GetDelayTillNextAttemptImpl() OVERRIDE {
    int delay;
    if (IsFastAttempt())
      delay = kFastDelayBetweenAttemptsSec;
    else if (IsNormalAttempt())
      delay = kNormalDelayBetweenAttemptsSec;
    else
      delay = kSlowDelayBetweenAttemptsSec;
    return AdjustDelay(base::TimeDelta::FromSeconds(delay));
  }
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() OVERRIDE {
    int timeout;
    if (IsFastAttempt())
      timeout = kFastAttemptTimeoutSec;
    else if (IsNormalAttempt())
      timeout = kNormalAttemptTimeoutSec;
    else
      timeout = kSlowAttemptTimeoutSec;
    return base::TimeDelta::FromSeconds(timeout);
  }

 private:
  bool IsFastAttempt() {
    return delegate_->AttemptCount() < kMaxFastAttempts;
  }

  bool IsNormalAttempt() {
    return delegate_->AttemptCount() < kMaxFastAttempts + kMaxNormalAttempts;
  }

  DISALLOW_COPY_AND_ASSIGN(SessionStrategy);
};

}  // namespace

// PortalDetectorStrategy -----------------------------------------------------

// static
base::TimeDelta PortalDetectorStrategy::delay_till_next_attempt_for_testing_;

// static
bool PortalDetectorStrategy::delay_till_next_attempt_for_testing_initialized_ =
    false;

// static
base::TimeDelta PortalDetectorStrategy::next_attempt_timeout_for_testing_;

// static
bool PortalDetectorStrategy::next_attempt_timeout_for_testing_initialized_ =
    false;

PortalDetectorStrategy::PortalDetectorStrategy() : delegate_(NULL) {}

PortalDetectorStrategy::~PortalDetectorStrategy() {}

// statc
scoped_ptr<PortalDetectorStrategy> PortalDetectorStrategy::CreateById(
    StrategyId id) {
  switch (id) {
    case STRATEGY_ID_LOGIN_SCREEN:
      return scoped_ptr<PortalDetectorStrategy>(new LoginScreenStrategy());
    case STRATEGY_ID_ERROR_SCREEN:
      return scoped_ptr<PortalDetectorStrategy>(new ErrorScreenStrategy());
    case STRATEGY_ID_SESSION:
      return scoped_ptr<PortalDetectorStrategy>(new SessionStrategy());
    default:
      NOTREACHED();
      return scoped_ptr<PortalDetectorStrategy>(
          static_cast<PortalDetectorStrategy*>(NULL));
  }
}

bool PortalDetectorStrategy::CanPerformAttempt() {
  return CanPerformAttemptImpl();
}

bool PortalDetectorStrategy::CanPerformAttemptAfterDetection() {
  return CanPerformAttemptAfterDetectionImpl();
}

base::TimeDelta PortalDetectorStrategy::GetDelayTillNextAttempt() {
  if (delay_till_next_attempt_for_testing_initialized_)
    return delay_till_next_attempt_for_testing_;
  return GetDelayTillNextAttemptImpl();
}

base::TimeDelta PortalDetectorStrategy::GetNextAttemptTimeout() {
  if (next_attempt_timeout_for_testing_initialized_)
    return next_attempt_timeout_for_testing_;
  return GetNextAttemptTimeoutImpl();
}

bool PortalDetectorStrategy::CanPerformAttemptImpl() { return false; }

bool PortalDetectorStrategy::CanPerformAttemptAfterDetectionImpl() {
  return false;
}

base::TimeDelta PortalDetectorStrategy::GetDelayTillNextAttemptImpl() {
  return base::TimeDelta();
}

base::TimeDelta PortalDetectorStrategy::GetNextAttemptTimeoutImpl() {
  return base::TimeDelta();
}

base::TimeDelta PortalDetectorStrategy::AdjustDelay(
    const base::TimeDelta& delay) {
  if (!delegate_->AttemptCount())
    return base::TimeDelta();

  base::TimeTicks now = delegate_->GetCurrentTimeTicks();
  base::TimeDelta elapsed;
  if (now > delegate_->AttemptStartTime())
    elapsed = now - delegate_->AttemptStartTime();
  if (delay > elapsed)
    return delay - elapsed;
  return base::TimeDelta();
}

}  // namespace chromeos
