// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_converters.h"

#include "base/logging.h"

namespace mojo {

// TODO(iclelland): Make these enums equivalent so that conversion can be a
// static cast.
content::ServiceWorkerStatusCode
TypeConverter<content::ServiceWorkerStatusCode,
              blink::mojom::ServiceWorkerEventStatus>::
    Convert(blink::mojom::ServiceWorkerEventStatus status) {
  content::ServiceWorkerStatusCode status_code;
  if (status == blink::mojom::ServiceWorkerEventStatus::COMPLETED) {
    status_code = content::SERVICE_WORKER_OK;
  } else if (status == blink::mojom::ServiceWorkerEventStatus::REJECTED) {
    status_code = content::SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;
  } else if (status == blink::mojom::ServiceWorkerEventStatus::ABORTED) {
    status_code = content::SERVICE_WORKER_ERROR_ABORT;
  } else {
    // We received an unexpected value back. This can theoretically happen as
    // mojo doesn't validate enum values.
    status_code = content::SERVICE_WORKER_ERROR_IPC_FAILED;
  }
  return status_code;
}

blink::WebPaymentAppRequest
TypeConverter<blink::WebPaymentAppRequest,
              payments::mojom::PaymentAppRequestPtr>::
    Convert(const payments::mojom::PaymentAppRequestPtr& input) {
  blink::WebPaymentAppRequest output;

  output.origin = blink::WebString::fromUTF8(input->origin.spec());

  output.methodData =
      blink::WebVector<blink::WebPaymentMethodData>(input->methodData.size());
  for (size_t i = 0; i < input->methodData.size(); i++) {
    output.methodData[i] = mojo::ConvertTo<blink::WebPaymentMethodData>(
        std::move(input->methodData[i]));
  }

  output.total = mojo::ConvertTo<blink::WebPaymentItem>(input->total);

  output.modifiers = blink::WebVector<blink::WebPaymentDetailsModifier>(
      input->modifiers.size());
  for (size_t i = 0; i < input->modifiers.size(); i++) {
    output.modifiers[i] =
        mojo::ConvertTo<blink::WebPaymentDetailsModifier>(input->modifiers[i]);
  }

  output.optionId = blink::WebString::fromUTF8(input->optionId);

  return output;
}

blink::WebPaymentMethodData
TypeConverter<blink::WebPaymentMethodData,
              payments::mojom::PaymentMethodDataPtr>::
    Convert(const payments::mojom::PaymentMethodDataPtr& input) {
  blink::WebPaymentMethodData output;

  output.supportedMethods =
      blink::WebVector<blink::WebString>(input->supported_methods.size());
  for (size_t i = 0; i < input->supported_methods.size(); i++) {
    output.supportedMethods[i] =
        blink::WebString::fromUTF8(input->supported_methods[i]);
  }

  output.stringifiedData = blink::WebString::fromUTF8(input->stringified_data);

  return output;
}

blink::WebPaymentItem
TypeConverter<blink::WebPaymentItem, payments::mojom::PaymentItemPtr>::Convert(
    const payments::mojom::PaymentItemPtr& input) {
  blink::WebPaymentItem output;
  output.label = blink::WebString::fromUTF8(input->label);
  output.amount =
      mojo::ConvertTo<blink::WebPaymentCurrencyAmount>(input->amount);
  output.pending = input->pending;
  return output;
}

blink::WebPaymentCurrencyAmount
TypeConverter<blink::WebPaymentCurrencyAmount,
              payments::mojom::PaymentCurrencyAmountPtr>::
    Convert(const payments::mojom::PaymentCurrencyAmountPtr& input) {
  blink::WebPaymentCurrencyAmount output;
  output.currency = blink::WebString::fromUTF8(input->currency);
  output.value = blink::WebString::fromUTF8(input->value);
  output.currencySystem = blink::WebString::fromUTF8(input->currency_system);
  return output;
}

blink::WebPaymentDetailsModifier
TypeConverter<blink::WebPaymentDetailsModifier,
              payments::mojom::PaymentDetailsModifierPtr>::
    Convert(const payments::mojom::PaymentDetailsModifierPtr& input) {
  blink::WebPaymentDetailsModifier output;

  output.supportedMethods = blink::WebVector<blink::WebString>(
      input->method_data->supported_methods.size());
  for (size_t i = 0; i < input->method_data->supported_methods.size(); i++) {
    output.supportedMethods[i] =
        blink::WebString::fromUTF8(input->method_data->supported_methods[i]);
  }

  output.total = mojo::ConvertTo<blink::WebPaymentItem>(input->total);

  output.additionalDisplayItems = blink::WebVector<blink::WebPaymentItem>(
      input->additional_display_items.size());
  for (size_t i = 0; i < input->additional_display_items.size(); i++) {
    output.additionalDisplayItems[i] = mojo::ConvertTo<blink::WebPaymentItem>(
        input->additional_display_items[i]);
  }

  output.stringifiedData =
      blink::WebString::fromUTF8(input->method_data->stringified_data);

  return output;
}

}  // namespace
