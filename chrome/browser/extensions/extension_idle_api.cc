// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_idle_api.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_idle_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"

namespace keys = extension_idle_api_constants;

namespace {

const int kIdlePollInterval = 1;  // Number of seconds between status checks
                                  // when polling for active.
const int kThrottleInterval = 1;  // Number of seconds to throttle idle checks
                                  // for. Return the previously checked idle
                                  // state if the next check is faster than this
const int kMinThreshold = 15;  // In seconds.  Set >1 sec for security concerns.
const int kMaxThreshold = 4*60*60;  // Four hours, in seconds. Not set
                                    // arbitrarily high for security concerns.
const unsigned int kMaxCacheSize = 100;  // Number of state queries to cache.

// Calculates the error query interval has in respect to idle interval.
// The error is defined as amount of the query interval that is not part of the
// idle interval.
double QueryErrorFromIdle(double idle_start,
                          double idle_end,
                          double query_start,
                          double query_end) {
  return query_end - idle_end + std::max(0., idle_start - query_start);
}

// Internal class which is used to poll for changes in the system idle state.
class ExtensionIdlePollingTask {
 public:
  explicit ExtensionIdlePollingTask(int threshold, IdleState last_state,
      Profile* profile) : threshold_(threshold), last_state_(last_state),
                          profile_(profile) {}

  // Check if we're active; then restart the polling task. Do this till we are
  // are in active state.
  void CheckIdleState();
  void IdleStateCallback(IdleState state);

  // Create a poll task to check for Idle state
  static void CreateNewPollTask(int threshold, IdleState state,
                                Profile* profile);

 private:
  int threshold_;
  IdleState last_state_;
  Profile* profile_;

  static bool poll_task_running_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIdlePollingTask);
};

// Implementation of ExtensionIdlePollingTask.
bool ExtensionIdlePollingTask::poll_task_running_ = false;

void ExtensionIdlePollingTask::IdleStateCallback(IdleState current_state) {
  // If we just came into an active state, notify the extension.
  if (IDLE_STATE_ACTIVE == current_state && last_state_ != current_state)
    ExtensionIdleEventRouter::OnIdleStateChange(profile_, current_state);

  ExtensionIdlePollingTask::poll_task_running_ = false;

  ExtensionIdleCache::UpdateCache(threshold_, current_state);

  // Startup another polling task as we exit.
  if (current_state != IDLE_STATE_ACTIVE)
    ExtensionIdlePollingTask::CreateNewPollTask(threshold_, current_state,
                                                profile_);

  // This instance won't be needed anymore.
  delete this;
}

void ExtensionIdlePollingTask::CheckIdleState() {
  CalculateIdleState(threshold_,
      base::Bind(&ExtensionIdlePollingTask::IdleStateCallback,
                 base::Unretained(this)));
}

// static
void ExtensionIdlePollingTask::CreateNewPollTask(int threshold, IdleState state,
                                                 Profile* profile) {
  if (ExtensionIdlePollingTask::poll_task_running_) return;

  ExtensionIdlePollingTask::poll_task_running_ = true;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExtensionIdlePollingTask::CheckIdleState, base::Unretained(
          new ExtensionIdlePollingTask(threshold, state, profile))),
      base::TimeDelta::FromSeconds(kIdlePollInterval));
}


const char* IdleStateToDescription(IdleState state) {
  if (IDLE_STATE_ACTIVE == state)
    return keys::kStateActive;
  if (IDLE_STATE_IDLE == state)
    return keys::kStateIdle;
  return keys::kStateLocked;
};

// Helper function for reporting the idle state.  The lifetime of the object
// returned is controlled by the caller.
StringValue* CreateIdleValue(IdleState idle_state) {
  StringValue* result = new StringValue(IdleStateToDescription(idle_state));
  return result;
}

int CheckThresholdBounds(int timeout) {
  if (timeout < kMinThreshold) return kMinThreshold;
  if (timeout > kMaxThreshold) return kMaxThreshold;
  return timeout;
}

};  // namespace

void ExtensionIdleEventRouter::OnIdleStateChange(Profile* profile,
                                                 IdleState state) {
  // Prepare the single argument of the current state.
  ListValue args;
  args.Append(CreateIdleValue(state));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      keys::kOnStateChanged, json_args, profile, GURL(), EventFilteringInfo());
}

bool ExtensionIdleQueryStateFunction::RunImpl() {
  int threshold;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = CheckThresholdBounds(threshold);

  IdleState state = ExtensionIdleCache::CalculateIdleState(threshold);
  if (state != IDLE_STATE_UNKNOWN) {
    result_.reset(CreateIdleValue(state));
    SendResponse(true);
    return true;
  }

  CalculateIdleState(threshold,
      base::Bind(&ExtensionIdleQueryStateFunction::IdleStateCallback,
                 this, threshold));
  // Don't send the response, it'll be sent by our callback
  return true;
}

void ExtensionIdleQueryStateFunction::IdleStateCallback(int threshold,
                                                        IdleState state) {
  // If our state is not active, make sure we're running a polling task to check
  // for active state and report it when it changes to active.
  if (state != IDLE_STATE_ACTIVE) {
    ExtensionIdlePollingTask::CreateNewPollTask(threshold, state, profile_);
  }

  result_.reset(CreateIdleValue(state));

  ExtensionIdleCache::UpdateCache(threshold, state);

  SendResponse(true);
}

ExtensionIdleCache::CacheData ExtensionIdleCache::cached_data =
    {-1, -1, -1, -1};

IdleState ExtensionIdleCache::CalculateIdleState(int threshold) {
  return CalculateState(threshold, base::Time::Now().ToDoubleT());
}

void ExtensionIdleCache::UpdateCache(int threshold, IdleState state) {
  Update(threshold, state, base::Time::Now().ToDoubleT());
}

IdleState ExtensionIdleCache::CalculateState(int threshold, double now) {
  if (threshold < kMinThreshold)
    return IDLE_STATE_UNKNOWN;
  double threshold_moment = now - static_cast<double>(threshold);
  double throttle_interval = static_cast<double>(kThrottleInterval);

  // We test for IDEL_STATE_LOCKED first, because the result should be
  // independent of the data for idle and active state. If last state was
  // LOCKED and test for LOCKED is satisfied we should always return LOCKED.
  if (cached_data.latest_locked > 0 &&
      now - cached_data.latest_locked < throttle_interval)
    return IDLE_STATE_LOCKED;

  // If thershold moment is beyond the moment after whih we are certain we have
  // been active, return active state. We allow kThrottleInterval error.
  if (cached_data.latest_known_active > 0 &&
      threshold_moment - cached_data.latest_known_active < throttle_interval)
    return IDLE_STATE_ACTIVE;

  // If total error that query interval has in respect to last recorded idle
  // interval is less than kThrottleInterval, return IDLE state.
  // query interval is the interval [now, now - threshold] and the error is
  // defined as amount of query interval that is outside of idle interval.
  double error_from_idle =
      QueryErrorFromIdle(cached_data.idle_interval_start,
          cached_data.idle_interval_end, threshold_moment, now);
  if (cached_data.idle_interval_end > 0 &&
      error_from_idle < throttle_interval)
    return IDLE_STATE_IDLE;

  return IDLE_STATE_UNKNOWN;
}

void ExtensionIdleCache::Update(int threshold, IdleState state, double now) {
  if (threshold < kMinThreshold)
    return;
  double threshold_moment = now - static_cast<double>(threshold);
  switch (state) {
    case IDLE_STATE_IDLE:
      if (threshold_moment > cached_data.idle_interval_end) {
        // Cached and new interval don't overlap. We disregard the cached one.
        cached_data.idle_interval_start = threshold_moment;
      } else {
        // Cached and new interval overlap, so we can combine them. We set
        // the cached interval begining to less recent one.
        cached_data.idle_interval_start =
            std::min(cached_data.idle_interval_start, threshold_moment);
      }
      cached_data.idle_interval_end = now;
      // Reset data for LOCKED state, since the last query result is not
      // LOCKED.
      cached_data.latest_locked = -1;
      break;
    case IDLE_STATE_ACTIVE:
      if (threshold_moment > cached_data.latest_known_active)
        cached_data.latest_known_active = threshold_moment;
      // Reset data for LOCKED state, since the last query result is not
      // LOCKED.
      cached_data.latest_locked = -1;
      break;
    case IDLE_STATE_LOCKED:
      if (threshold_moment > cached_data.latest_locked)
        cached_data.latest_locked = now;
      break;
    default:
      return;
  }
}

int ExtensionIdleCache::get_min_threshold() {
  return kMinThreshold;
}

double ExtensionIdleCache::get_throttle_interval() {
  return static_cast<double>(kThrottleInterval);
}
