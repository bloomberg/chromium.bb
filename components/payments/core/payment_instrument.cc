// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_instrument.h"

namespace payments {

PaymentInstrument::PaymentInstrument(const std::string& method_name,
                                     const base::string16& label,
                                     const base::string16& sublabel,
                                     int icon_resource_id)
    : method_name_(method_name),
      label_(label),
      sublabel_(sublabel),
      icon_resource_id_(icon_resource_id) {}

PaymentInstrument::~PaymentInstrument() {}

}  // namespace payments
