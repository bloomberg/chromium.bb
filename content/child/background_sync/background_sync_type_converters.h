// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_
#define CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncError.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace mojo {

// blink::WebSyncRegistration::NetworkState <=>
//     content::mojom::BackgroundSyncNetworkState

template <>
struct CONTENT_EXPORT
    TypeConverter<blink::WebSyncRegistration::NetworkState,
                  content::mojom::BackgroundSyncNetworkState> {
  static blink::WebSyncRegistration::NetworkState Convert(
      content::mojom::BackgroundSyncNetworkState input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::mojom::BackgroundSyncNetworkState,
                                    blink::WebSyncRegistration::NetworkState> {
  static content::mojom::BackgroundSyncNetworkState Convert(
      blink::WebSyncRegistration::NetworkState input);
};

// blink::WebSyncRegistration <=>
//     content::mojom::SyncRegistration

template <>
struct CONTENT_EXPORT TypeConverter<scoped_ptr<blink::WebSyncRegistration>,
                                    content::mojom::SyncRegistrationPtr> {
  static scoped_ptr<blink::WebSyncRegistration> Convert(
      const content::mojom::SyncRegistrationPtr& input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::mojom::SyncRegistrationPtr,
                                    blink::WebSyncRegistration> {
  static content::mojom::SyncRegistrationPtr Convert(
      const blink::WebSyncRegistration& input);
};

// blink::WebServiceWorkerContextProxy::LastChanceOption <=>
//    content::mojom::BackgroundSyncEventLastChance

template <>
struct CONTENT_EXPORT
    TypeConverter<blink::WebServiceWorkerContextProxy::LastChanceOption,
                  content::mojom::BackgroundSyncEventLastChance> {
  static blink::WebServiceWorkerContextProxy::LastChanceOption Convert(
      content::mojom::BackgroundSyncEventLastChance input);
};

template <>
struct CONTENT_EXPORT
    TypeConverter<content::mojom::BackgroundSyncEventLastChance,
                  blink::WebServiceWorkerContextProxy::LastChanceOption> {
  static content::mojom::BackgroundSyncEventLastChance Convert(
      blink::WebServiceWorkerContextProxy::LastChanceOption input);
};

}  // namespace mojo

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_
