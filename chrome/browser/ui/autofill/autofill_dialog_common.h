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

// Returns true if |input| should be shown when |field_type| has been requested.
bool InputTypeMatchesFieldType(const DetailInput& input,
                               const AutofillType& field_type);

// Returns true if |input| in the given |section| should be used for a
// site-requested |field|.
bool DetailInputMatchesField(DialogSection section,
                             const DetailInput& input,
                             const AutofillField& field);

// Returns true if the |type| belongs to the CREDIT_CARD field type group.
bool IsCreditCardType(ServerFieldType type);

// Constructs |inputs| from template data for a given |dialog_section|.
void BuildInputsForSection(DialogSection dialog_section, DetailInputs* inputs);

// Returns the AutofillMetrics::DIALOG_UI_*_ITEM_ADDED metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiItemAddedEvent(
    DialogSection section);

// Returns the AutofillMetrics::DIALOG_UI_*_ITEM_ADDED metric corresponding
// to the |section|.
AutofillMetrics::DialogUiEvent DialogSectionToUiSelectionChangedEvent(
    DialogSection section);

// We hardcode some values. In particular, we don't yet allow the user to change
// the country: http://crbug.com/247518
string16 GetHardcodedValueForType(ServerFieldType type);

}  // namespace common
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMMON_H_
