// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implementation supposes a single extension thread and synchronized
// method invokation.

#include "chrome/browser/extensions/extension_idle_api.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
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

const int kIdlePollInterval = 15;  // Number of seconds between status checks
                                   // when polling for active.
const int kMinThreshold = 15;  // In seconds.  Set >1 sec for security concerns.
const int kMaxThreshold = 60*60;  // One hours, in seconds.  Not set arbitrarily
                                  // high for security concerns.

struct ExtensionIdlePollingData {
  IdleState state;
  double timestamp;
};

// Static variables shared between instances of polling.
static ExtensionIdlePollingData polling_data;

// Forward declaration of utility methods.
static const char* IdleStateToDescription(IdleState state);
static StringValue* CreateIdleValue(IdleState idle_state);
static int CheckThresholdBounds(int timeout);
static IdleState CalculateIdleStateAndUpdateTimestamp(int threshold);
static void CreateNewPollTask(Profile* profile);
static IdleState ThrottledCalculateIdleState(int threshold, Profile* profile);

// Internal object which watches for changes in the system idle state.
class ExtensionIdlePollingTask : public Task {
 public:
  explicit ExtensionIdlePollingTask(Profile* profile) : profile_(profile) {}
  virtual ~ExtensionIdlePollingTask() {}

  // Overridden from Task.
  virtual void Run();

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIdlePollingTask);
};

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

IdleState CalculateIdleStateAndUpdateTimestamp(int threshold) {
  polling_data.timestamp = base::Time::Now().ToDoubleT();
  return CalculateIdleState(threshold);
}

void CreateNewPollTask(Profile* profile) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      new ExtensionIdlePollingTask(profile),
      kIdlePollInterval * 1000);
}

IdleState ThrottledCalculateIdleState(int threshold, Profile* profile) {
  // If we are not active we should be polling.
  if (IDLE_STATE_ACTIVE != polling_data.state)
    return polling_data.state;

  // Only allow one check per threshold.
  double time_now = base::Time::Now().ToDoubleT();
  double delta = time_now - polling_data.timestamp;
  if (delta < threshold)
    return polling_data.state;

  // Update the new state with a poll.  Note this updates time of last check.
  polling_data.state = CalculateIdleStateAndUpdateTimestamp(threshold);

  if (IDLE_STATE_ACTIVE != polling_data.state)
    CreateNewPollTask(profile);

  return polling_data.state;
}

void ExtensionIdlePollingTask::Run() {
  IdleState state = CalculateIdleStateAndUpdateTimestamp(
      kIdlePollInterval);
  if (state != polling_data.state) {
    polling_data.state = state;

    // Inform of change if the current state is IDLE_STATE_ACTIVE.
    if (IDLE_STATE_ACTIVE == polling_data.state)
      ExtensionIdleEventRouter::OnIdleStateChange(profile_, state);
  }

  // Create a secondary polling task until an active state is reached.
  if (IDLE_STATE_ACTIVE != polling_data.state)
    CreateNewPollTask(profile_);
}

};  // namespace

bool ExtensionIdleQueryStateFunction::RunImpl() {
  int threshold;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = CheckThresholdBounds(threshold);
  IdleState state = ThrottledCalculateIdleState(threshold, profile());
  result_.reset(CreateIdleValue(state));
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
