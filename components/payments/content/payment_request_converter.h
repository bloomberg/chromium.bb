// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_CONVERTER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_CONVERTER_H_

#include "third_party/WebKit/public/platform/modules/payments/payment_request.mojom.h"

// TODO(crbug.com/760945): Write unit tests for these functions.

namespace payments {

class PaymentDetails;
class PaymentMethodData;

PaymentMethodData ConvertPaymentMethodData(
    const mojom::PaymentMethodDataPtr& method_data_entry);

PaymentDetails ConvertPaymentDetails(
    const mojom::PaymentDetailsPtr& details_entry);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_CONVERTER_H_
