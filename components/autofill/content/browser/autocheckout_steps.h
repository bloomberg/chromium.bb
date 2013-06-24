// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_STEPS_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_STEPS_H_

namespace autofill {

// Stages of a buy flow that may be encountered on an Autocheckout-supported
// site, used primarily for display purposes.
// Indexed for easy conversion from int values returned by Autofill server.
enum AutocheckoutStepType {
  AUTOCHECKOUT_STEP_MIN_VALUE = 1,
  AUTOCHECKOUT_STEP_SHIPPING = AUTOCHECKOUT_STEP_MIN_VALUE,
  AUTOCHECKOUT_STEP_DELIVERY = 2,
  AUTOCHECKOUT_STEP_BILLING = 3,
  AUTOCHECKOUT_STEP_PROXY_CARD = 4,
  AUTOCHECKOUT_STEP_MAX_VALUE = AUTOCHECKOUT_STEP_PROXY_CARD,
};

// Possible statuses for the above step types, again used primarily for display.
enum AutocheckoutStepStatus {
  AUTOCHECKOUT_STEP_UNSTARTED,
  AUTOCHECKOUT_STEP_STARTED,
  AUTOCHECKOUT_STEP_COMPLETED,
  AUTOCHECKOUT_STEP_FAILED,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_STEPS_H_
