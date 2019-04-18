// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_CONTACT_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_CONTACT_FORM_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// with name, address, email, and phone fields.
class AddressContactFormLabelFormatter : public LabelFormatter {
 public:
  AddressContactFormLabelFormatter(
      const std::string& app_locale,
      ServerFieldType focused_field_type,
      uint32_t groups,
      const std::vector<ServerFieldType>& field_types,
      bool show_phone,
      bool show_email);

  ~AddressContactFormLabelFormatter() override;

  base::string16 GetLabelForProfile(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const override;

 private:
  // True if this formatter's associated form has a street address field. A
  // form may have an address-related field, e.g. zip code, without having a
  // street address field. If a form does not include a street address field,
  // street addresses should not appear in labels.
  bool form_has_street_address_;

  // True if phone numbers should be included in labels.
  bool show_phone_;

  // True if email addresses should be included in labels.
  bool show_email_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_CONTACT_FORM_LABEL_FORMATTER_H_
