// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PHONE_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PHONE_FORM_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// with name, address, and phone fields and without email fields.
class AddressPhoneFormLabelFormatter : public LabelFormatter {
 public:
  AddressPhoneFormLabelFormatter(
      const std::string& app_locale,
      FieldTypeGroup focused_group,
      const std::vector<ServerFieldType>& field_types);

  ~AddressPhoneFormLabelFormatter() override;

  std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PHONE_FORM_LABEL_FORMATTER_H_
