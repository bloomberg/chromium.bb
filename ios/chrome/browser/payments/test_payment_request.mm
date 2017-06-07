// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/test_payment_request.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/region_data_loader.h"
#include "ios/web/public/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

autofill::RegionDataLoader* TestPaymentRequest::GetRegionDataLoader() {
  return region_data_loader_;
}
