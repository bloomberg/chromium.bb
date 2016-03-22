
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
                content::mojom::BackgroundSyncNetworkState::ANY));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                content::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                content::mojom::BackgroundSyncNetworkState::ONLINE));
}

TEST(BackgroundSyncTypeConverterTest, TestMojoToBlinkNetworkStateConversions) {
  ASSERT_EQ(content::mojom::BackgroundSyncNetworkState::ANY,
            ConvertTo<content::mojom::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateAny));
  ASSERT_EQ(content::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR,
            ConvertTo<content::mojom::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateAvoidCellular));
  ASSERT_EQ(content::mojom::BackgroundSyncNetworkState::ONLINE,
            ConvertTo<content::mojom::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateOnline));
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultBlinkToMojoConversion) {
  blink::WebSyncRegistration in;
  content::mojom::SyncRegistrationPtr out =
      ConvertTo<content::mojom::SyncRegistrationPtr>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->id);
  ASSERT_EQ("", out->tag);
  ASSERT_EQ(content::mojom::BackgroundSyncNetworkState::ONLINE,
            out->network_state);
}

TEST(BackgroundSyncTypeConverterTest, TestFullBlinkToMojoConversion) {
  blink::WebSyncRegistration in(
      7, "BlinkToMojo", blink::WebSyncRegistration::NetworkStateAvoidCellular);
  content::mojom::SyncRegistrationPtr out =
      ConvertTo<content::mojom::SyncRegistrationPtr>(in);

  ASSERT_EQ(7, out->id);
  ASSERT_EQ("BlinkToMojo", out->tag);
  ASSERT_EQ(content::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR,
            out->network_state);
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultMojoToBlinkConversion) {
  content::mojom::SyncRegistrationPtr in(
      content::mojom::SyncRegistration::New());
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->id);
  ASSERT_EQ("",out->tag);
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline, out->networkState);
}

TEST(BackgroundSyncTypeConverterTest, TestFullMojoToBlinkConversion) {
  content::mojom::SyncRegistrationPtr in(
      content::mojom::SyncRegistration::New());
  in->id = 41;
  in->tag = mojo::String("MojoToBlink");
  in->network_state =
      content::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR;
  scoped_ptr<blink::WebSyncRegistration> out =
      ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(41, out->id);
  ASSERT_EQ("MojoToBlink", out->tag.utf8());
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            out->networkState);
}

}  // anonymous namespace
}  // namespace mojo

