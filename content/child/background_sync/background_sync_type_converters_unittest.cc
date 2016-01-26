
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(BackgroundSyncTypeConverterTest, TestBlinkToMojoNetworkStateConversions) {
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAny,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                content::BackgroundSyncNetworkState::ANY));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                content::BackgroundSyncNetworkState::AVOID_CELLULAR));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                content::BackgroundSyncNetworkState::ONLINE));
}

TEST(BackgroundSyncTypeConverterTest, TestMojoToBlinkNetworkStateConversions) {
  ASSERT_EQ(content::BackgroundSyncNetworkState::ANY,
            ConvertTo<content::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateAny));
  ASSERT_EQ(content::BackgroundSyncNetworkState::AVOID_CELLULAR,
            ConvertTo<content::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateAvoidCellular));
  ASSERT_EQ(content::BackgroundSyncNetworkState::ONLINE,
            ConvertTo<content::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateOnline));
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultBlinkToMojoConversion) {
  blink::WebSyncRegistration in;
  content::SyncRegistrationPtr out =
      ConvertTo<content::SyncRegistrationPtr>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->handle_id);
  ASSERT_EQ("", out->tag);
  ASSERT_EQ(content::BackgroundSyncNetworkState::ONLINE, out->network_state);
}

TEST(BackgroundSyncTypeConverterTest, TestFullBlinkToMojoConversion) {
  blink::WebSyncRegistration in(
      7, "BlinkToMojo", blink::WebSyncRegistration::NetworkStateAvoidCellular);
  content::SyncRegistrationPtr out =
      ConvertTo<content::SyncRegistrationPtr>(in);

  ASSERT_EQ(7, out->handle_id);
  ASSERT_EQ("BlinkToMojo", out->tag);
  ASSERT_EQ(content::BackgroundSyncNetworkState::AVOID_CELLULAR,
            out->network_state);
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultMojoToBlinkConversion) {
  content::SyncRegistrationPtr in(
      content::SyncRegistration::New());
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->id);
  ASSERT_EQ("",out->tag);
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline, out->networkState);
}

TEST(BackgroundSyncTypeConverterTest, TestFullMojoToBlinkConversion) {
  content::SyncRegistrationPtr in(
      content::SyncRegistration::New());
  in->handle_id = 41;
  in->tag = mojo::String("MojoToBlink");
  in->network_state = content::BackgroundSyncNetworkState::AVOID_CELLULAR;
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(41, out->id);
  ASSERT_EQ("MojoToBlink", out->tag.utf8());
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            out->networkState);
}

}  // anonymous namespace
}  // namespace mojo

