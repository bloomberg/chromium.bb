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

// blink::WebSyngRegistration::Periodicity <=>
//    content::BackgroundSyncPeriodicity

template <>
struct CONTENT_EXPORT TypeConverter<blink::WebSyncRegistration::Periodicity,
                     content::BackgroundSyncPeriodicity> {
  static blink::WebSyncRegistration::Periodicity Convert(
      content::BackgroundSyncPeriodicity input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::BackgroundSyncPeriodicity,
                     blink::WebSyncRegistration::Periodicity> {
  static content::BackgroundSyncPeriodicity Convert(
      blink::WebSyncRegistration::Periodicity input);
};

// blink::WebSyncRegistration::NetworkState <=>
//     content::BackgroundSyncNetworkState

template <>
struct CONTENT_EXPORT TypeConverter<blink::WebSyncRegistration::NetworkState,
                     content::BackgroundSyncNetworkState> {
  static blink::WebSyncRegistration::NetworkState Convert(
      content::BackgroundSyncNetworkState input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::BackgroundSyncNetworkState,
                     blink::WebSyncRegistration::NetworkState> {
  static content::BackgroundSyncNetworkState Convert(
      blink::WebSyncRegistration::NetworkState input);
};

// blink::WebSyncRegistration::PowerState <=>
//     content::BackgroundSyncPowerState

template <>
struct CONTENT_EXPORT TypeConverter<blink::WebSyncRegistration::PowerState,
                     content::BackgroundSyncPowerState> {
  static blink::WebSyncRegistration::PowerState Convert(
      content::BackgroundSyncPowerState input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::BackgroundSyncPowerState,
                     blink::WebSyncRegistration::PowerState> {
  static content::BackgroundSyncPowerState Convert(
      blink::WebSyncRegistration::PowerState input);
};

// blink::WebSyncRegistration <=>
//     content::SyncRegistration

template <>
struct CONTENT_EXPORT TypeConverter<scoped_ptr<blink::WebSyncRegistration>,
                     content::SyncRegistrationPtr> {
  static scoped_ptr<blink::WebSyncRegistration> Convert(
      const content::SyncRegistrationPtr& input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::SyncRegistrationPtr,
                     blink::WebSyncRegistration> {
  static content::SyncRegistrationPtr Convert(
      const blink::WebSyncRegistration& input);
};

// blink::WebServiceWorkerContextProxy::LastChanceOption <=>
//    content::BackgroundSyncEventLastChance

template <>
struct CONTENT_EXPORT
    TypeConverter<blink::WebServiceWorkerContextProxy::LastChanceOption,
                  content::BackgroundSyncEventLastChance> {
  static blink::WebServiceWorkerContextProxy::LastChanceOption Convert(
      content::BackgroundSyncEventLastChance input);
};

template <>
struct CONTENT_EXPORT
    TypeConverter<content::BackgroundSyncEventLastChance,
                  blink::WebServiceWorkerContextProxy::LastChanceOption> {
  static content::BackgroundSyncEventLastChance Convert(
      blink::WebServiceWorkerContextProxy::LastChanceOption input);
};

}  // namespace mojo

#endif  // CONTENT_CHILD_BACKGROUND_SYNC_BACKGROUND_SYNC_TYPE_CONVERTERS_H_
