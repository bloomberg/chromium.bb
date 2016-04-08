// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_type_converters.h"

#include "base/logging.h"

namespace mojo {

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, blink_name) \
  static_assert(static_cast<int>(content::mojo_name) ==     \
                    static_cast<int>(blink::blink_name),    \
                "mojo and blink enums must match")

COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncNetworkState::ANY,
                             WebSyncRegistration::NetworkStateAny);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncNetworkState::AVOID_CELLULAR,
                             WebSyncRegistration::NetworkStateAvoidCellular);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncNetworkState::ONLINE,
                             WebSyncRegistration::NetworkStateOnline);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncNetworkState::MAX,
                             WebSyncRegistration::NetworkStateOnline);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncNetworkState::MAX,
                             WebSyncRegistration::NetworkStateLast);
COMPILE_ASSERT_MATCHING_ENUM(
    mojom::BackgroundSyncEventLastChance::IS_NOT_LAST_CHANCE,
    WebServiceWorkerContextProxy::IsNotLastChance);
COMPILE_ASSERT_MATCHING_ENUM(
    mojom::BackgroundSyncEventLastChance::IS_LAST_CHANCE,
    WebServiceWorkerContextProxy::IsLastChance);

// static
blink::WebSyncRegistration::NetworkState
TypeConverter<blink::WebSyncRegistration::NetworkState,
              content::mojom::BackgroundSyncNetworkState>::
    Convert(content::mojom::BackgroundSyncNetworkState input) {
  return static_cast<blink::WebSyncRegistration::NetworkState>(input);
}

// static
content::mojom::BackgroundSyncNetworkState
TypeConverter<content::mojom::BackgroundSyncNetworkState,
              blink::WebSyncRegistration::NetworkState>::
    Convert(blink::WebSyncRegistration::NetworkState input) {
  return static_cast<content::mojom::BackgroundSyncNetworkState>(input);
}

// static
std::unique_ptr<blink::WebSyncRegistration>
TypeConverter<std::unique_ptr<blink::WebSyncRegistration>,
              content::mojom::SyncRegistrationPtr>::
    Convert(const content::mojom::SyncRegistrationPtr& input) {
  std::unique_ptr<blink::WebSyncRegistration> result(
      new blink::WebSyncRegistration());
  result->id = input->id;
  result->tag = blink::WebString::fromUTF8(input->tag);
  result->networkState =
      ConvertTo<blink::WebSyncRegistration::NetworkState>(input->network_state);
  return result;
}

// static
content::mojom::SyncRegistrationPtr
TypeConverter<content::mojom::SyncRegistrationPtr, blink::WebSyncRegistration>::
    Convert(const blink::WebSyncRegistration& input) {
  content::mojom::SyncRegistrationPtr result(
      content::mojom::SyncRegistration::New());
  result->id = input.id;
  result->tag = input.tag.utf8();
  result->network_state =
      ConvertTo<content::mojom::BackgroundSyncNetworkState>(input.networkState);
  return result;
}

// static
blink::WebServiceWorkerContextProxy::LastChanceOption
TypeConverter<blink::WebServiceWorkerContextProxy::LastChanceOption,
              content::mojom::BackgroundSyncEventLastChance>::
    Convert(content::mojom::BackgroundSyncEventLastChance input) {
  return static_cast<blink::WebServiceWorkerContextProxy::LastChanceOption>(
      input);
}

// static
content::mojom::BackgroundSyncEventLastChance
TypeConverter<content::mojom::BackgroundSyncEventLastChance,
              blink::WebServiceWorkerContextProxy::LastChanceOption>::
    Convert(blink::WebServiceWorkerContextProxy::LastChanceOption input) {
  return static_cast<content::mojom::BackgroundSyncEventLastChance>(input);
}

}  // namespace mojo
