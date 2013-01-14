// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOFILL_AUTOCHECKOUT_STATUS_H_
#define CHROME_COMMON_AUTOFILL_AUTOCHECKOUT_STATUS_H_

namespace autofill {

enum AutocheckoutStatus {
  SUCCESS,
  MISSING_FIELDMAPPING,
  MISSING_ADVANCE,
  CANNOT_PROCEED,
};

}  // namespace autofill

#endif  // CHROME_COMMON_AUTOFILL_AUTOCHECKOUT_STATUS_H_
