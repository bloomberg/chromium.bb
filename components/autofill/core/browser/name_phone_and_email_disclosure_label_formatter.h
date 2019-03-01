// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NAME_PHONE_AND_EMAIL_DISCLOSURE_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NAME_PHONE_AND_EMAIL_DISCLOSURE_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter.h"

namespace autofill {

// A LabelFormatter that uses the disclosure approach to create Suggestions'
// disambiguating labels for forms exclusively containing name, phone, and email
// fields.
class NamePhoneAndEmailDisclosureLabelFormatter : public LabelFormatter {
 public:
  explicit NamePhoneAndEmailDisclosureLabelFormatter(
      const std::string& app_locale,
      ServerFieldType focused_field_type,
      const std::vector<ServerFieldType>& field_types);
  ~NamePhoneAndEmailDisclosureLabelFormatter() override;

  std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const override;

 private:
  // The locale for which to generate labels. This reflects the language and
  // country for which the application is translated, e.g. en_AU for Austalian
  // English.
  std::string app_locale_;

  // The field on which the user is currently focused.
  ServerFieldType focused_field_type_;

  // A collection of meaningful field types in the form with which the user is
  // interacting. The NO_SERVER_DATA and UNKNOWN_TYPE field types are not
  // considered meaningful.
  std::vector<ServerFieldType> field_types_;

  // A collection of meaningful field types excluding the focused_field_type_.
  // These types are used to construct the labels.
  std::vector<ServerFieldType> filtered_field_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NAME_PHONE_AND_EMAIL_DISCLOSURE_LABEL_FORMATTER_H_
