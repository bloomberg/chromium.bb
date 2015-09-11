// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(BackgroundSyncTypeConverterTest, TestBlinkToMojoPeriodicityConversions) {
  ASSERT_EQ(blink::WebSyncRegistration::PeriodicityPeriodic,
      ConvertTo<blink::WebSyncRegistration::Periodicity>(
          content::BACKGROUND_SYNC_PERIODICITY_PERIODIC));
  ASSERT_EQ(blink::WebSyncRegistration::PeriodicityOneShot,
      ConvertTo<blink::WebSyncRegistration::Periodicity>(
          content::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT));
}

TEST(BackgroundSyncTypeConverterTest, TestMojoToBlinkPeriodicityConversions) {
  ASSERT_EQ(content::BACKGROUND_SYNC_PERIODICITY_PERIODIC,
      ConvertTo<content::BackgroundSyncPeriodicity>(
          blink::WebSyncRegistration::PeriodicityPeriodic));
  ASSERT_EQ(content::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT,
      ConvertTo<content::BackgroundSyncPeriodicity>(
          blink::WebSyncRegistration::PeriodicityOneShot));
}

TEST(BackgroundSyncTypeConverterTest, TestBlinkToMojoNetworkStateConversions) {
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAny,
      ConvertTo<blink::WebSyncRegistration::NetworkState>(
          content::BACKGROUND_SYNC_NETWORK_STATE_ANY));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
      ConvertTo<blink::WebSyncRegistration::NetworkState>(
          content::BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline,
      ConvertTo<blink::WebSyncRegistration::NetworkState>(
          content::BACKGROUND_SYNC_NETWORK_STATE_ONLINE));
}

TEST(BackgroundSyncTypeConverterTest, TestMojoToBlinkNetworkStateConversions) {
  ASSERT_EQ(content::BACKGROUND_SYNC_NETWORK_STATE_ANY,
      ConvertTo<content::BackgroundSyncNetworkState>(
          blink::WebSyncRegistration::NetworkStateAny));
  ASSERT_EQ(content::BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR,
      ConvertTo<content::BackgroundSyncNetworkState>(
          blink::WebSyncRegistration::NetworkStateAvoidCellular));
  ASSERT_EQ(content::BACKGROUND_SYNC_NETWORK_STATE_ONLINE,
      ConvertTo<content::BackgroundSyncNetworkState>(
          blink::WebSyncRegistration::NetworkStateOnline));
}

TEST(BackgroundSyncTypeConverterTest, TestBlinkToMojoPowerStateConversions) {
  ASSERT_EQ(blink::WebSyncRegistration::PowerStateAuto,
      ConvertTo<blink::WebSyncRegistration::PowerState>(
          content::BACKGROUND_SYNC_POWER_STATE_AUTO));
  ASSERT_EQ(blink::WebSyncRegistration::PowerStateAvoidDraining,
      ConvertTo<blink::WebSyncRegistration::PowerState>(
          content::BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING));
}

TEST(BackgroundSyncTypeConverterTest, TestMojoToBlinkPowerStateConversions) {
  ASSERT_EQ(content::BACKGROUND_SYNC_POWER_STATE_AUTO,
      ConvertTo<content::BackgroundSyncPowerState>(
          blink::WebSyncRegistration::PowerStateAuto));
  ASSERT_EQ(content::BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING,
      ConvertTo<content::BackgroundSyncPowerState>(
          blink::WebSyncRegistration::PowerStateAvoidDraining));
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultBlinkToMojoConversion) {
  blink::WebSyncRegistration in;
  content::SyncRegistrationPtr out =
      ConvertTo<content::SyncRegistrationPtr>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->handle_id);
  ASSERT_EQ(content::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT, out->periodicity);
  ASSERT_EQ("", out->tag);
  ASSERT_EQ(0UL, out->min_period_ms);
  ASSERT_EQ(content::BACKGROUND_SYNC_NETWORK_STATE_ONLINE, out->network_state);
  ASSERT_EQ(content::BACKGROUND_SYNC_POWER_STATE_AUTO, out->power_state);
}

TEST(BackgroundSyncTypeConverterTest, TestFullPeriodicBlinkToMojoConversion) {
  blink::WebSyncRegistration in(
      7, blink::WebSyncRegistration::PeriodicityPeriodic, "BlinkToMojo",
      12340000, blink::WebSyncRegistration::NetworkStateAvoidCellular,
      blink::WebSyncRegistration::PowerStateAvoidDraining);
  content::SyncRegistrationPtr out =
      ConvertTo<content::SyncRegistrationPtr>(in);

  ASSERT_EQ(7, out->handle_id);
  ASSERT_EQ(content::BACKGROUND_SYNC_PERIODICITY_PERIODIC, out->periodicity);
  ASSERT_EQ("BlinkToMojo", out->tag);
  ASSERT_EQ(12340000UL, out->min_period_ms);
  ASSERT_EQ(content::BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR,
            out->network_state);
  ASSERT_EQ(content::BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING,
            out->power_state);
}

TEST(BackgroundSyncTypeConverterTest, TestFullOneShotBlinkToMojoConversion) {
  blink::WebSyncRegistration in(
      7, blink::WebSyncRegistration::PeriodicityOneShot, "BlinkToMojo",
      12340000, blink::WebSyncRegistration::NetworkStateAvoidCellular,
      blink::WebSyncRegistration::PowerStateAvoidDraining);
  content::SyncRegistrationPtr out =
      ConvertTo<content::SyncRegistrationPtr>(in);

  ASSERT_EQ(7, out->handle_id);
  ASSERT_EQ(content::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT, out->periodicity);
  ASSERT_EQ("BlinkToMojo", out->tag);
  ASSERT_EQ(12340000UL, out->min_period_ms);
  ASSERT_EQ(content::BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR,
            out->network_state);
  ASSERT_EQ(content::BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING,
            out->power_state);
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultMojoToBlinkConversion) {
  content::SyncRegistrationPtr in(
      content::SyncRegistration::New());
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->id);
  ASSERT_EQ(blink::WebSyncRegistration::PeriodicityOneShot, out->periodicity);
  ASSERT_EQ("",out->tag);
  ASSERT_EQ(0UL, out->minPeriodMs);
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline, out->networkState);
  ASSERT_EQ(blink::WebSyncRegistration::PowerStateAuto, out->powerState);
}

TEST(BackgroundSyncTypeConverterTest, TestFullPeriodicMojoToBlinkConversion) {
  content::SyncRegistrationPtr in(
      content::SyncRegistration::New());
  in->handle_id = 41;
  in->periodicity = content::BACKGROUND_SYNC_PERIODICITY_PERIODIC;
  in->tag = mojo::String("MojoToBlink");
  in->min_period_ms = 43210000;
  in->network_state = content::BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR;
  in->power_state = content::BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING;
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(41, out->id);
  ASSERT_EQ(blink::WebSyncRegistration::PeriodicityPeriodic, out->periodicity);
  ASSERT_EQ("MojoToBlink", out->tag.utf8());
  ASSERT_EQ(43210000UL, out->minPeriodMs);
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            out->networkState);
  ASSERT_EQ(blink::WebSyncRegistration::PowerStateAvoidDraining,
            out->powerState);
}

TEST(BackgroundSyncTypeConverterTest, TestFullOneShotMojoToBlinkConversion) {
  content::SyncRegistrationPtr in(
      content::SyncRegistration::New());
  in->handle_id = 41;
  in->periodicity = content::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT;
  in->tag = mojo::String("MojoToBlink");
  in->min_period_ms = 43210000;
  in->network_state = content::BACKGROUND_SYNC_NETWORK_STATE_AVOID_CELLULAR;
  in->power_state = content::BACKGROUND_SYNC_POWER_STATE_AVOID_DRAINING;
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(41, out->id);
  ASSERT_EQ(blink::WebSyncRegistration::PeriodicityOneShot, out->periodicity);
  ASSERT_EQ("MojoToBlink", out->tag.utf8());
  ASSERT_EQ(43210000UL, out->minPeriodMs);
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            out->networkState);
  ASSERT_EQ(blink::WebSyncRegistration::PowerStateAvoidDraining,
            out->powerState);
}

}  // anonymous namespace
}  // namespace mojo

