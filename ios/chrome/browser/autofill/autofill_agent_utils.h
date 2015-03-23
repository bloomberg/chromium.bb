// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AGENT_UTILS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AGENT_UTILS_H_

// TODO (sgrant): Switch to componentized version of this code when
// http://crbug/328070 is fixed.

namespace autofill {
class FormStructure;
}

namespace autofill_agent_util {

// Determines if the |structure| has any fields that are of type
// autofill::CREDIT_CARD and thus asking for credit card info.
bool RequestingCreditCardInfo(const autofill::FormStructure* structure);

// Returns true if one of the nodes in |structure| request information related
// to a billing address.
bool RequestFullBillingAddress(autofill::FormStructure* structure);

// Returns true if one of the nodes in |structure| request information related
// to a shipping address. To determine this actually attempt to fill the form
// using an empty data model that tracks which fields are requested.
bool RequestShippingAddress(autofill::FormStructure* structure);

// Returns true if one of the nodes in |structure| request information related
// to a phone number.
bool RequestPhoneNumber(autofill::FormStructure* structure);

}  // namespace autofill_agent_util

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AGENT_UTILS_H_
