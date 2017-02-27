// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_

#include "components/payments/content/payment_app.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/platform/modules/payments/WebPaymentAppRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

namespace mojo {

template <>
struct CONTENT_EXPORT TypeConverter<content::ServiceWorkerStatusCode,
                                    blink::mojom::ServiceWorkerEventStatus> {
  static content::ServiceWorkerStatusCode Convert(
      blink::mojom::ServiceWorkerEventStatus status);
};

template <>
struct TypeConverter<blink::WebPaymentAppRequest,
                     payments::mojom::PaymentAppRequestPtr> {
  static blink::WebPaymentAppRequest Convert(
      const payments::mojom::PaymentAppRequestPtr& input);
};

template <>
struct TypeConverter<blink::WebPaymentMethodData,
                     payments::mojom::PaymentMethodDataPtr> {
  static blink::WebPaymentMethodData Convert(
      const payments::mojom::PaymentMethodDataPtr& input);
};

template <>
struct TypeConverter<blink::WebPaymentItem, payments::mojom::PaymentItemPtr> {
  static blink::WebPaymentItem Convert(
      const payments::mojom::PaymentItemPtr& input);
};

template <>
struct TypeConverter<blink::WebPaymentCurrencyAmount,
                     payments::mojom::PaymentCurrencyAmountPtr> {
  static blink::WebPaymentCurrencyAmount Convert(
      const payments::mojom::PaymentCurrencyAmountPtr& input);
};

template <>
struct TypeConverter<blink::WebPaymentDetailsModifier,
                     payments::mojom::PaymentDetailsModifierPtr> {
  static blink::WebPaymentDetailsModifier Convert(
      const payments::mojom::PaymentDetailsModifierPtr& input);
};

}  // namespace

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
