// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter.h"

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

// Returns a subset of meaningful ServerFieldTypes found in |field_types|.
// ServerFieldTypes like NO_SERVER_DATA and UNKNOWN_TYPE are frequently found in
// the collection of types sent from the frontend. Other types, e.g.
// COMPANY_NAME and PHONE_FAX_WHOLE_NUMBER, are also sometimes present. These
// types are not useful to LabelFormatters, so only types related to names,
// addresses, emails, and phone numbers are in the results.
std::vector<ServerFieldType> FilterFieldTypes(
    const std::vector<ServerFieldType>& field_types);

// Returns a bitmask indicating whether the NAME, ADDRESS_HOME, EMAIL, and
// PHONE_HOME FieldTypeGroups are associated with the given |field_types|.
uint32_t DetermineGroups(const std::vector<ServerFieldType>& field_types);

// Creates a form-specific LabelFormatter according to |field_types|. If the
// given |field_types| do not correspond to a LabelFormatter, then nullptr will
// be returned.
std::unique_ptr<LabelFormatter> Create(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_UTILS_H_
