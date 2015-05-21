// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/card_unmask_delegate.h"

namespace autofill {

CardUnmaskDelegate::UnmaskResponse::UnmaskResponse()
    : should_store_pan(false),
      providing_risk_advisory_data(false) {
#if defined(OS_IOS)
  // On iOS, we generate a RiskAdvisoryData instead of the
  // BrowserNativeFingerprinting produced on other platforms. This field
  // directs the Wallet client to configure the request accordingly.
  providing_risk_advisory_data = true;
#endif
}

CardUnmaskDelegate::UnmaskResponse::~UnmaskResponse() {}

}  // namespace autofill
