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

class LoginScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kBaseAttemptTimeoutSec = 5;
  static const int kMaxAttemptTimeoutSec = 30;

  explicit LoginScreenStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}
  virtual ~LoginScreenStrategy() {}

 protected:
  // PortalDetectorStrategy overrides:
  virtual StrategyId Id() const OVERRIDE { return STRATEGY_ID_LOGIN_SCREEN; }
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() OVERRIDE {
    if (DefaultNetwork() && delegate_->NoResponseResultCount() != 0) {
      int timeout = kMaxAttemptTimeoutSec;
      if (kMaxAttemptTimeoutSec / (delegate_->NoResponseResultCount() + 1) >
          kBaseAttemptTimeoutSec) {
        timeout =
            kBaseAttemptTimeoutSec * (delegate_->NoResponseResultCount() + 1);
      }
      return base::TimeDelta::FromSeconds(timeout);
    }
    return base::TimeDelta::FromSeconds(kBaseAttemptTimeoutSec);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenStrategy);
};

class ErrorScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kAttemptTimeoutSec = 15;

  explicit ErrorScreenStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}
  virtual ~ErrorScreenStrategy() {}

 protected:
  // PortalDetectorStrategy overrides:
  virtual StrategyId Id() const OVERRIDE { return STRATEGY_ID_ERROR_SCREEN; }
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() OVERRIDE {
    return base::TimeDelta::FromSeconds(kAttemptTimeoutSec);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ErrorScreenStrategy);
};

class SessionStrategy : public PortalDetectorStrategy {
 public:
  static const int kMaxFastAttempts = 3;
  static const int kFastAttemptTimeoutSec = 3;
  static const int kSlowAttemptTimeoutSec = 5;

  explicit SessionStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}
  virtual ~SessionStrategy() {}

 protected:
  virtual StrategyId Id() const OVERRIDE { return STRATEGY_ID_SESSION; }
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() OVERRIDE {
    int timeout;
    if (delegate_->NoResponseResultCount() < kMaxFastAttempts)
      timeout = kFastAttemptTimeoutSec;
    else
      timeout = kSlowAttemptTimeoutSec;
    return base::TimeDelta::FromSeconds(timeout);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStrategy);
};

}  // namespace

// PortalDetectorStrategy::BackoffEntryImpl ------------------------------------

class PortalDetectorStrategy::BackoffEntryImpl : public net::BackoffEntry {
 public:
  BackoffEntryImpl(const net::BackoffEntry::Policy* const policy,
                   PortalDetectorStrategy::Delegate* delegate)
      : net::BackoffEntry(policy), delegate_(delegate) {}
  virtual ~BackoffEntryImpl() {}

  // net::BackoffEntry overrides:
  virtual base::TimeTicks ImplGetTimeNow() const OVERRIDE {
    return delegate_->GetCurrentTimeTicks();
  }

 private:
  PortalDetectorStrategy::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BackoffEntryImpl);
};

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

PortalDetectorStrategy::PortalDetectorStrategy(Delegate* delegate)
    : delegate_(delegate) {
  // First |policy_.num_errors_to_ignore| attempts with the same
  // result are performed with |policy_.initial_delay_ms| between
  // them. Delay before every consecutive attempt is multplied by
  // |policy_.multiply_factor_|. Also, |policy_.jitter_factor| is used
  // for each delay.
  policy_.num_errors_to_ignore = 3;
  policy_.initial_delay_ms = 600;
  policy_.multiply_factor = 2.0;
  policy_.jitter_factor = 0.3;
  policy_.maximum_backoff_ms = 2 * 60 * 1000;
  policy_.entry_lifetime_ms = -1;
  policy_.always_use_initial_delay = true;
  backoff_entry_.reset(new BackoffEntryImpl(&policy_, delegate_));
}

PortalDetectorStrategy::~PortalDetectorStrategy() {
}

// statc
scoped_ptr<PortalDetectorStrategy> PortalDetectorStrategy::CreateById(
    StrategyId id,
    Delegate* delegate) {
  switch (id) {
    case STRATEGY_ID_LOGIN_SCREEN:
      return scoped_ptr<PortalDetectorStrategy>(
          new LoginScreenStrategy(delegate));
    case STRATEGY_ID_ERROR_SCREEN:
      return scoped_ptr<PortalDetectorStrategy>(
          new ErrorScreenStrategy(delegate));
    case STRATEGY_ID_SESSION:
      return scoped_ptr<PortalDetectorStrategy>(new SessionStrategy(delegate));
    default:
      NOTREACHED();
      return scoped_ptr<PortalDetectorStrategy>(
          static_cast<PortalDetectorStrategy*>(NULL));
  }
}

base::TimeDelta PortalDetectorStrategy::GetDelayTillNextAttempt() {
  if (delay_till_next_attempt_for_testing_initialized_)
    return delay_till_next_attempt_for_testing_;
  return backoff_entry_->GetTimeUntilRelease();
}

base::TimeDelta PortalDetectorStrategy::GetNextAttemptTimeout() {
  if (next_attempt_timeout_for_testing_initialized_)
    return next_attempt_timeout_for_testing_;
  return GetNextAttemptTimeoutImpl();
}

void PortalDetectorStrategy::Reset() {
  backoff_entry_->Reset();
}

void PortalDetectorStrategy::SetPolicyAndReset(
    const net::BackoffEntry::Policy& policy) {
  policy_ = policy;
  backoff_entry_.reset(new BackoffEntryImpl(&policy_, delegate_));
}

void PortalDetectorStrategy::OnDetectionCompleted() {
  backoff_entry_->InformOfRequest(false);
}

base::TimeDelta PortalDetectorStrategy::GetNextAttemptTimeoutImpl() {
  return base::TimeDelta();
}

}  // namespace chromeos
