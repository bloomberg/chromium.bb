// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_I18N_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_I18N_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

namespace i18n {
namespace addressinput {
struct AddressData;
}
}

namespace autofill {

class AutofillProfile;
class AutofillType;

namespace i18n {

// Creates an AddressData object for internationalized address display or
// validation using |get_info| for field values.
scoped_ptr< ::i18n::addressinput::AddressData> CreateAddressData(
    const base::Callback<base::string16(const AutofillType&)>& get_info);

// Creates an |AddressData| from |profile|.
scoped_ptr< ::i18n::addressinput::AddressData>
    CreateAddressDataFromAutofillProfile(const AutofillProfile& profile,
                                         const std::string& app_locale);

}  // namespace i18n
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_I18N_H_
