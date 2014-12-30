// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_

#include "base/strings/string16.h"

namespace autofill {

class CardUnmaskDelegate {
 public:
  virtual void OnUnmaskResponse(const base::string16& cvc) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_
