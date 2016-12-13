// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"

namespace autofill {

std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate) {
  // TODO(jdonnelly): Implement iOS save card infobar. http://crbug.com/535784
  return ::CreateConfirmInfoBar(std::move(delegate));
}

}  // namespace autofill
