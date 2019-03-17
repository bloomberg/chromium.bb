// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
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

// Used to separate the parts of a label that should appear on the same line,
// for example, (202) 456-1111 • george.washington@gmail.com.
constexpr wchar_t kLabelDelimiter[] = L" • ";

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

// Returns the national address associated with |profile|, e.g.
// 24 Beacon St., Boston, MA 02133.
base::string16 GetLabelNationalAddress(
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

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
