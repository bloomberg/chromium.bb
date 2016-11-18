// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_type_converters.h"

#include "base/logging.h"

namespace mojo {

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, blink_name) \
  static_assert(static_cast<int>(blink::mojo_name) ==       \
                    static_cast<int>(blink::blink_name),    \
                "mojo and blink enums must match")

COMPILE_ASSERT_MATCHING_ENUM(
    mojom::BackgroundSyncEventLastChance::IS_NOT_LAST_CHANCE,
    WebServiceWorkerContextProxy::IsNotLastChance);
COMPILE_ASSERT_MATCHING_ENUM(
    mojom::BackgroundSyncEventLastChance::IS_LAST_CHANCE,
    WebServiceWorkerContextProxy::IsLastChance);

// static
blink::WebServiceWorkerContextProxy::LastChanceOption
TypeConverter<blink::WebServiceWorkerContextProxy::LastChanceOption,
              blink::mojom::BackgroundSyncEventLastChance>::
    Convert(blink::mojom::BackgroundSyncEventLastChance input) {
  return static_cast<blink::WebServiceWorkerContextProxy::LastChanceOption>(
      input);
}

// static
blink::mojom::BackgroundSyncEventLastChance
TypeConverter<blink::mojom::BackgroundSyncEventLastChance,
              blink::WebServiceWorkerContextProxy::LastChanceOption>::
    Convert(blink::WebServiceWorkerContextProxy::LastChanceOption input) {
  return static_cast<blink::mojom::BackgroundSyncEventLastChance>(input);
}

}  // namespace mojo
