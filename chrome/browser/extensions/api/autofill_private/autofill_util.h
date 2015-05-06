// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}

namespace extensions {

namespace autofill_util {

using AddressEntryList = std::vector<linked_ptr<
    extensions::api::autofill_private::AddressEntry> >;
using CreditCardEntryList = std::vector<linked_ptr<
    extensions::api::autofill_private::CreditCardEntry> >;

// Uses |personal_data| to generate a list of up-to-date AddressEntry objects.
scoped_ptr<AddressEntryList> GenerateAddressList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to generate a list of up-to-date CreditCardEntry
// objects.
scoped_ptr<CreditCardEntryList> GenerateCreditCardList(
    const autofill::PersonalDataManager& personal_data);

}  // namespace autofill_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
