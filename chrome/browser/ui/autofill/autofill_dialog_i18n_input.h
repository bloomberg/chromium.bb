// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_I18N_INPUT_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_I18N_INPUT_H_

#include "chrome/browser/ui/autofill/autofill_dialog_common.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

namespace autofill {
namespace i18ninput {

// Returns true if the internationalized address input is enabled.
bool Enabled();

// Builds internationalized address input fields for |country_code| and adds
// them (at most 13) to |inputs|. |address_type| is which kind of address to
// build (e.g. billing or shipping).
void BuildAddressInputs(common::AddressType address_type,
                        const std::string& country_code,
                        DetailInputs* inputs);

}  // namespace i18ninput
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_I18N_INPUT_H_
