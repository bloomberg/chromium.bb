// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCHECKOUT_STATUS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCHECKOUT_STATUS_H_

namespace autofill {

enum AutocheckoutStatus {
  SUCCESS,
  MISSING_FIELDMAPPING,
  MISSING_ADVANCE,
  MISSING_CLICK_ELEMENT_BEFORE_FORM_FILLING,
  MISSING_CLICK_ELEMENT_AFTER_FORM_FILLING,
  CANNOT_PROCEED,
  AUTOCHECKOUT_STATUS_NUM_STATUS,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCHECKOUT_STATUS_H_
