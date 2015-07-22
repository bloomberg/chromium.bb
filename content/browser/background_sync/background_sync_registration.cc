// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_registration.h"

namespace mojo {

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, browser_name) \
  COMPILE_ASSERT(static_cast<int>(content::mojo_name) ==      \
                     static_cast<int>(content::browser_name), \
                 mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_PERIODIC,
                             SYNC_PERIODIC);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_ONE_SHOT,
                             SYNC_ONE_SHOT);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_MAX, SYNC_ONE_SHOT);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_MAX,
                             SyncPeriodicity_MAX);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_ANY,
                             NETWORK_STATE_ANY);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR,
                             NETWORK_STATE_AVOID_CELLULAR);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_ONLINE,
                             NETWORK_STATE_ONLINE);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_MAX,
                             NETWORK_STATE_ONLINE);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_MAX,
                             SyncNetworkState_MAX);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_AUTO,
                             POWER_STATE_AUTO);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING,
                             POWER_STATE_AVOID_DRAINING);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_MAX,
                             POWER_STATE_AVOID_DRAINING);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_MAX,
                             SyncPowerState_MAX);

// static
scoped_ptr<content::BackgroundSyncRegistration> TypeConverter<
    scoped_ptr<content::BackgroundSyncRegistration>,
    content::SyncRegistrationPtr>::Convert(const content::SyncRegistrationPtr&
                                               in) {
  scoped_ptr<content::BackgroundSyncRegistration> out(
      new content::BackgroundSyncRegistration());
  out->set_id(in->id);
  content::BackgroundSyncRegistrationOptions* options = out->options();
  options->tag = in->tag;
  options->min_period = in->min_period_ms;
  options->power_state = static_cast<content::SyncPowerState>(in->power_state);
  options->network_state =
      static_cast<content::SyncNetworkState>(in->network_state);
  options->periodicity = static_cast<content::SyncPeriodicity>(in->periodicity);
  return out;
}

// static
content::SyncRegistrationPtr
TypeConverter<content::SyncRegistrationPtr,
              content::BackgroundSyncRegistration>::
    Convert(const content::BackgroundSyncRegistration& in) {
  content::SyncRegistrationPtr out(content::SyncRegistration::New());
  out->id = in.id();
  const content::BackgroundSyncRegistrationOptions* options = in.options();
  out->tag = options->tag;
  out->min_period_ms = options->min_period;
  out->periodicity =
      static_cast<content::BackgroundSyncPeriodicity>(options->periodicity);
  out->power_state =
      static_cast<content::BackgroundSyncPowerState>(options->power_state);
  out->network_state =
      static_cast<content::BackgroundSyncNetworkState>(options->network_state);
  return out.Pass();
}

}  // namespace

namespace content {

// TODO(thakis): Remove this once http://crbug.com/488634 is fixed.
BackgroundSyncRegistration::BackgroundSyncRegistration() = default;

const BackgroundSyncRegistration::RegistrationId
    BackgroundSyncRegistration::kInvalidRegistrationId = -1;

const BackgroundSyncRegistration::RegistrationId
    BackgroundSyncRegistration::kInitialId = 0;

bool BackgroundSyncRegistration::Equals(
    const BackgroundSyncRegistration& other) const {
  return options_.Equals(other.options_);
}

bool BackgroundSyncRegistration::IsValid() const {
  return id_ != kInvalidRegistrationId;
}

}  // namespace content
