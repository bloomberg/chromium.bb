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
// The form contains at least one field associated with the NAME_HOME or
// NAME_BILLING FieldTypeGroups.
constexpr uint32_t kName = 1 << 0;
// The form contains at least one field associated with the ADDRESS_HOME or
// ADDRESS_BILLING FieldTypeGroups.
constexpr uint32_t kAddress = 1 << 1;
// The form contains at least one field associated with the EMAIL
// FieldTypeGroup.
constexpr uint32_t kEmail = 1 << 2;
// The form contains at least one field associated with the PHONE_HOME or
// PHONE_BILLING FieldTypeGroup.
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

// Returns true if |types| has a street-address-related field.
bool HasStreetAddress(const std::vector<ServerFieldType>& types);

// Adds |part| to |parts| if |part| is not an empty string.
void AddLabelPartIfNotEmpty(const base::string16& part,
                            std::vector<base::string16>* parts);

// Returns the text to be shown to the user. |parts| may have 0, 1, or 2
// elements.
//
// If |parts| is empty, then an empty string is returned. This might happen
// when (A) a profile has only name and address information and (B) the user
// associated with this profile interacts with a contact form, which includes
// name, email address, and phone number fields and excludes address fields.
//
// If |parts| has a single element, then the string is returned. This might
// happen when (A) a profile has only name and email address information and
// (B) the user associated with this profile interacts with a contact form.
base::string16 ConstructLabelLine(const std::vector<base::string16>& parts);

// Returns a bitmask indicating whether the NAME, ADDRESS_HOME, EMAIL, and
// PHONE_HOME FieldTypeGroups are associated with the given |field_types|.
uint32_t DetermineGroups(const std::vector<ServerFieldType>& field_types);

// Returns a pared down copy of |profile|. The copy has the same guid, origin,
// country and language codes, and |field_types| as |profile|.
AutofillProfile MakeTrimmedProfile(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& field_types);

// Returns the full name associated with |profile|.
base::string16 GetLabelName(const AutofillProfile& profile,
                            const std::string& app_locale);

// If |form_has_street_address_| is true and |profile| is associated with a
// street address, then returns |profile|'s street address, e.g. 24 Beacon St.
//
// If |form_has_street_address_| is false and |profile| is associated with
// address fields other than street addresses, then returns the address-related
// data corresponding to |types|.
base::string16 GetLabelAddress(bool form_has_street_address_,
                               const AutofillProfile& profile,
                               const std::string& app_locale,
                               const std::vector<ServerFieldType>& types);

// Returns the national address associated with |profile|, e.g.
// 24 Beacon St., Boston, MA 02133.
base::string16 GetLabelNationalAddress(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& field_types);

// Returns the street address associated with |profile|, e.g.
// 24 Beacon St.
base::string16 GetLabelStreetAddress(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& field_types);

// Returns the email address associated with |profile|, if any; otherwise,
// returns an empty string.
base::string16 GetLabelEmail(const AutofillProfile& profile,
                             const std::string& app_locale);

// Returns the phone number associated with |profile|, if any; otherwise,
// returns an empty string. Phone numbers are given in |profile|'s country's
// national format, if possible.
base::string16 GetLabelPhone(const AutofillProfile& profile,
                             const std::string& app_locale);

// Returns a vector of only address-related field types in |types|.
std::vector<ServerFieldType> ExtractAddressFieldTypes(
    const std::vector<ServerFieldType>& types);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
