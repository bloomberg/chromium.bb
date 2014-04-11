// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMMON_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMMON_H_

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {
class AutofillProfile;
}

namespace wallet {
class Address;
}

namespace autofill {
namespace common {

// The types of addresses this class supports building.
enum AddressType {
  ADDRESS_TYPE_BILLING,
  ADDRESS_TYPE_SHIPPING,
};

// Returns true if |type| should be shown when |field_type| has been requested.
// This filters the types that we fill into the page to match the ones the
// dialog actually cares about, preventing rAc from giving away data that an
// AutofillProfile or other data source might know about the user which isn't
// represented in the dialog.
bool ServerTypeEncompassesFieldType(ServerFieldType type,
                                    const AutofillType& field_type);

// Returns true if |type| in the given |section| should be used for a
// site-requested |field|.
bool ServerTypeMatchesField(DialogSection section,
                            ServerFieldType type,
                            const AutofillField& field);

// Returns true if the |type| belongs to the CREDIT_CARD field type group.
bool IsCreditCardType(ServerFieldType type);

// Constructs |inputs| from template data for a given |dialog_section|.
// |country_country| specifies the country code that the inputs should be built
// for. Sets the |language_code| to be used for address formatting, if
// internationalized address input is enabled. The |language_code| parameter can
// be NULL.
void BuildInputsForSection(DialogSection dialog_section,
                           const std::string& country_code,
                           DetailInputs* inputs,
                           std::string* language_code);

// Returns the AutofillMetrics::DIALOG_UI_*_ITEM_ADDED metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiItemAddedEvent(
    DialogSection section);

// Returns the AutofillMetrics::DIALOG_UI_*_ITEM_ADDED metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiSelectionChangedEvent(
    DialogSection section);

// Gets just the |type| attributes from each DetailInput.
std::vector<ServerFieldType> TypesFromInputs(const DetailInputs& inputs);

}  // namespace common
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMMON_H_
