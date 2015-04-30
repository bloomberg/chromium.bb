// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_type_converters.h"

#include "base/logging.h"

namespace mojo {

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, blink_name)     \
  COMPILE_ASSERT(static_cast<int>(content::mojo_name) ==        \
      static_cast<int>(blink::WebSyncRegistration::blink_name), \
      mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_PERIODIC,
                             PeriodicityPeriodic);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_ONE_SHOT,
                             PeriodicityOneShot);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_MAX,
                             PeriodicityOneShot);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_PERIODICITY_MAX,
                             PeriodicityLast);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_ANY,
                             NetworkStateAny);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR,
                             NetworkStateAvoidCellular);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_ONLINE,
                             NetworkStateOnline);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_MAX,
                             NetworkStateOnline);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_NETWORK_STATE_MAX,
                             NetworkStateLast);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_AUTO,
                             PowerStateAuto);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING,
                             PowerStateAvoidDraining);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_MAX,
                             PowerStateAvoidDraining);
COMPILE_ASSERT_MATCHING_ENUM(BACKGROUND_SYNC_POWER_STATE_MAX,
                             PowerStateLast);

// static
blink::WebSyncRegistration::Periodicity
    TypeConverter<blink::WebSyncRegistration::Periodicity,
                  content::BackgroundSyncPeriodicity>::Convert(
        content::BackgroundSyncPeriodicity input) {
  return static_cast<blink::WebSyncRegistration::Periodicity>(input);
}

// static
content::BackgroundSyncPeriodicity
    TypeConverter<content::BackgroundSyncPeriodicity,
                  blink::WebSyncRegistration::Periodicity>::Convert(
        blink::WebSyncRegistration::Periodicity input) {
  return static_cast<content::BackgroundSyncPeriodicity>(input);
}

// static
blink::WebSyncRegistration::NetworkState
    TypeConverter<blink::WebSyncRegistration::NetworkState,
                  content::BackgroundSyncNetworkState>::Convert(
        content::BackgroundSyncNetworkState input) {
  return static_cast<blink::WebSyncRegistration::NetworkState>(input);
}

// static
content::BackgroundSyncNetworkState
    TypeConverter<content::BackgroundSyncNetworkState,
                  blink::WebSyncRegistration::NetworkState>::Convert(
        blink::WebSyncRegistration::NetworkState input) {
  return static_cast<content::BackgroundSyncNetworkState>(input);
}

// static
blink::WebSyncRegistration::PowerState
    TypeConverter<blink::WebSyncRegistration::PowerState,
                  content::BackgroundSyncPowerState>::Convert(
        content::BackgroundSyncPowerState input) {
  return static_cast<blink::WebSyncRegistration::PowerState>(input);
}

// static
content::BackgroundSyncPowerState
    TypeConverter<content::BackgroundSyncPowerState,
                  blink::WebSyncRegistration::PowerState>::Convert(
        blink::WebSyncRegistration::PowerState input) {
  return static_cast<content::BackgroundSyncPowerState>(input);
}

// static
scoped_ptr<blink::WebSyncRegistration> TypeConverter<
    scoped_ptr<blink::WebSyncRegistration>,
    content::SyncRegistrationPtr>::Convert(
         const content::SyncRegistrationPtr& input) {
  scoped_ptr<blink::WebSyncRegistration> result(
      new blink::WebSyncRegistration());
  result->id = input->id;
  result->periodicity =
      ConvertTo<blink::WebSyncRegistration::Periodicity>(input->periodicity);
  result->tag = blink::WebString::fromUTF8(input->tag);
  result->minPeriodMs = input->min_period_ms;
  result->networkState =
      ConvertTo<blink::WebSyncRegistration::NetworkState>(input->network_state);
  result->powerState =
      ConvertTo<blink::WebSyncRegistration::PowerState>(input->power_state);
  return result;
}

// static
content::SyncRegistrationPtr TypeConverter<
    content::SyncRegistrationPtr,
    blink::WebSyncRegistration>::Convert(
        const blink::WebSyncRegistration& input) {
  content::SyncRegistrationPtr result(
      content::SyncRegistration::New());
  result->id = input.id;
  result->periodicity =
      ConvertTo<content::BackgroundSyncPeriodicity>(input.periodicity);
  result->tag = input.tag.utf8();
  result->min_period_ms = input.minPeriodMs;
  result->network_state =
      ConvertTo<content::BackgroundSyncNetworkState>(input.networkState);
  result->power_state =
      ConvertTo<content::BackgroundSyncPowerState>(input.powerState);
  return result.Pass();
}

}  // namespace mojo
