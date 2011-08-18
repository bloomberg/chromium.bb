// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_idle_api.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_idle_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/renderer_host/render_view_host.h"

namespace keys = extension_idle_api_constants;

namespace {
const int kIdlePollInterval = 1;  // Number of seconds between status checks
                                  // when polling for active.
const int kThrottleInterval = 1;  // Number of seconds to throttle idle checks
                                  // for. Return the previously checked idle
                                  // state if the next check is faster than this
const int kMinThreshold = 15;  // In seconds.  Set >1 sec for security concerns.
const int kMaxThreshold = 60*60;  // One hours, in seconds.  Not set arbitrarily
                                  // high for security concerns.

struct ExtensionIdlePollingData {
  IdleState state;
  double timestamp;
};

// Used to throttle excessive calls to query for idle state
ExtensionIdlePollingData polling_data = {IDLE_STATE_UNKNOWN, 0};

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
          kIdlePollInterval * 1000);
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

bool ShouldThrottle() {
  double now = base::Time::Now().ToDoubleT();
  double delta = now - polling_data.timestamp;
  polling_data.timestamp = now;
  if (delta < kThrottleInterval)
    return false;
  else
    return true;
}
};  // namespace

void ExtensionIdleQueryStateFunction::IdleStateCallback(int threshold,
                                                        IdleState state) {
  // If our state is not active, make sure we're running a polling task to check
  // for active state and report it when it changes to active.
  if (state != IDLE_STATE_ACTIVE) {
    ExtensionIdlePollingTask::CreateNewPollTask(threshold, state, profile_);
  }

  result_.reset(CreateIdleValue(state));
  polling_data.state = state;
  SendResponse(true);
}

bool ExtensionIdleQueryStateFunction::RunImpl() {
  int threshold;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = CheckThresholdBounds(threshold);

  if (ShouldThrottle()) {
    if (polling_data.state != IDLE_STATE_UNKNOWN) {
      result_.reset(CreateIdleValue(polling_data.state));
      SendResponse(true);
      return true;
    }
    // We cannot get the idle state right now, we're already checking for idle
    // from a previous call, so continue with normal idle check instead.
  }

  polling_data.state = IDLE_STATE_UNKNOWN;
  CalculateIdleState(threshold,
      base::Bind(&ExtensionIdleQueryStateFunction::IdleStateCallback,
                 base::Unretained(this), threshold));

  // Don't send the response, it'll be sent by our callback
  return true;
}

void ExtensionIdleEventRouter::OnIdleStateChange(Profile* profile,
                                                 IdleState state) {
  // Prepare the single argument of the current state.
  ListValue args;
  args.Append(CreateIdleValue(state));
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      keys::kOnStateChanged, json_args, profile, GURL());
}
