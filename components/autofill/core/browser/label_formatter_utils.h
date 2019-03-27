// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {
namespace label_formatter_groups {

// Bits for FieldTypeGroup options.
// The form has a field associated with the NAME_HOME or NAME_BILLING
// FieldTypeGroups.
constexpr uint32_t kName = 1 << 0;
// The form has a field associated with the ADDRESS_HOME or ADDRESS_BILLING
// FieldTypeGroups.
constexpr uint32_t kAddress = 1 << 1;
// The form has a field associated with the EMAIL FieldTypeGroup.
constexpr uint32_t kEmail = 1 << 2;
// The form has a field associated with the PHONE_HOME or PHONE_BILLING
// FieldTypeGroups.
constexpr uint32_t kPhone = 1 << 3;

}  // namespace label_formatter_groups

const size_t kMaxNumberOfParts = 2;

// Returns true if kName is set in |groups|.
bool ContainsName(uint32_t groups);

// Returns true if kAddress is set in |groups|.
bool ContainsAddress(uint32_t groups);

// Returns true if kEmail is set in |groups|.
bool ContainsEmail(uint32_t groups);

// Returns true if kPhone is set in |groups|.
bool ContainsPhone(uint32_t groups);

// Returns a bitmask indicating whether the NAME, ADDRESS_HOME, EMAIL, and
// PHONE_HOME FieldTypeGroups are associated with the given |field_types|.
uint32_t DetermineGroups(const std::vector<ServerFieldType>& types);

// Returns true if |type| is a component of a street address.
bool IsStreetAddressPart(ServerFieldType type);

// Returns true if |types| has a street-address-related field.
bool HasStreetAddress(const std::vector<ServerFieldType>& types);

// Returns a vector of only street-address-related field types in |types| if
// |extract_street_address_types| is true, e.g. ADDRESS_HOME_LINE1.
//
// Returns a vector of only non-street-address-related field types in |types|
// if |extract_street_address_types| is false, e.g. ADDRESS_BILLING_ZIP.
std::vector<ServerFieldType> ExtractSpecifiedAddressFieldTypes(
    bool extract_street_address_types,
    const std::vector<ServerFieldType>& types);

// Adds |part| to |parts| if |part| is not an empty string.
void AddLabelPartIfNotEmpty(const base::string16& part,
                            std::vector<base::string16>* parts);

// Returns the text to show to the user. |parts| may have 0, 1, or 2 elements.
//
// If |parts| is empty, then an empty string is returned. This might happen
// when (A) a profile has only name and address information and (B) the user
// with this profile interacts with a contact form, which excludes address
// fields and includes name, email address, and phone number fields.
//
// If |parts| has a single element, then the string is returned. This might
// happen when (A) a profile has only name and email address information and
// (B) the user with this profile interacts with a contact form.
base::string16 ConstructLabelLine(const std::vector<base::string16>& parts);

// Returns a pared down copy of |profile|. The copy has the same guid, origin,
// country and language codes, and |field_types| as |profile|.
AutofillProfile MakeTrimmedProfile(const AutofillProfile& profile,
                                   const std::string& app_locale,
                                   const std::vector<ServerFieldType>& types);

// Returns the full name associated with |profile|.
base::string16 GetLabelName(const AutofillProfile& profile,
                            const std::string& app_locale);

// Returns either street-address data or non-street-address data found in
// |profile|. If |focused_field_type| is a street address field, then returns
// non-street-address data, e.g. Lowell, MA 01852.
//
// If the focused type is not a street address field and if |form_has_street_
// address| is true, then returns street-address data, e.g. 375 Merrimack St.
//
// If the focused type is not a street address field and if the form does not
// have a street address, then returns the parts of the address in the form
// other than the focused field. For example, if a user focuses on a city
// field and if the state and zip code can be in the label, then MA 01852 is
// returned. If there are no other non-street-address fields or if the data is
// not present in |profile|, then an empty string is returned.
base::string16 GetLabelForFocusedAddress(
    ServerFieldType focused_field_type,
    bool form_has_street_address,
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& types);

// If |form_has_street_address_| is true and if |profile| is associated with a
// street address, then returns the street address, e.g. 24 Beacon St.
//
// If |form_has_street_address_| is false and |profile| is associated with
// address fields other than street addresses, then returns the non-street-
// address-related data corresponding to |types|.
base::string16 GetLabelAddress(bool use_street_address,
                               const AutofillProfile& profile,
                               const std::string& app_locale,
                               const std::vector<ServerFieldType>& types);

// Returns the national address associated with |profile|, e.g.
// 24 Beacon St., Boston, MA 02133.
base::string16 GetLabelNationalAddress(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& types);

// Returns the street address associated with |profile|, e.g. 24 Beacon St.
base::string16 GetLabelStreetAddress(const AutofillProfile& profile,
                                     const std::string& app_locale,
                                     const std::vector<ServerFieldType>& types);

// Returns the email address associated with |profile|, if any; otherwise,
// returns an empty string.
base::string16 GetLabelEmail(const AutofillProfile& profile,
                             const std::string& app_locale);

// Returns the phone number associated with |profile|, if any; otherwise,
// returns an empty string. Phone numbers are given in |profile|'s country's
// national format, if possible.
base::string16 GetLabelPhone(const AutofillProfile& profile,
                             const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
