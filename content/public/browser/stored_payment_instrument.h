// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_INSTRUMENT_H_
#define CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_INSTRUMENT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// This class represents the stored payment instrument in UA.
struct CONTENT_EXPORT StoredPaymentInstrument {
  StoredPaymentInstrument();
  ~StoredPaymentInstrument();

  // Id of the service worker registration this instrument is associated with.
  int64_t registration_id = 0;

  // Id of this payment instrument. This key will be passed to the payment
  // handler to indicate the instrument selected by the user.
  std::string instrument_key;

  // Origin of the payment app provider that provides this payment instrument.
  GURL origin;

  // Label for this payment instrument.
  std::string name;

  // A list of one or more payment method identifiers of the payment methods
  // supported by this payment instrument.
  std::vector<std::string> enabled_methods;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_INSTRUMENT_H_
