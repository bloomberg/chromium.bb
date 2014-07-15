// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_I18N_INPUT_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_I18N_INPUT_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_dialog_common.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

class AutofillProfile;
class CreditCard;

namespace i18ninput {

// Builds internationalized address input fields for |address_type| (e.g.
// billing or shipping) in |country_code| (e.g. "US" or "CH").
//
// The |inputs| and |language_code| are output-only parameters.
//
// The function adds the fields (at most 13) to |inputs|. This parameter should
// not be NULL.
//
// The function sets the |language_code| to be used for address formatting. This
// parameter can be NULL.
void BuildAddressInputs(common::AddressType address_type,
                        const std::string& country_code,
                        DetailInputs* inputs,
                        std::string* language_code);

// Returns whether the given card is complete and verified (i.e. was reviewed
// by the user and not just automatically aggregated).
bool CardHasCompleteAndVerifiedData(const CreditCard& card);

// As above, but for the address in |profile|. Region-aware, meaning that the
// exact set of required fields depends on the region.
bool AddressHasCompleteAndVerifiedData(const AutofillProfile& profile,
                                       const std::string& app_locale);

}  // namespace i18ninput
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_I18N_INPUT_H_
