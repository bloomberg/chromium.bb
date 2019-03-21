// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Handles the creation of Suggestions' disambiguating labels.
class LabelFormatter {
 public:
  LabelFormatter(const std::string& app_locale,
                 ServerFieldType focused_field_type,
                 const std::vector<ServerFieldType>& field_types);
  virtual ~LabelFormatter();

  // Returns a collection of |labels| formed by extracting useful disambiguating
  // information from a collection of |profiles|.
  virtual std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const = 0;

  // Creates a form-specific LabelFormatter according to |field_types|. If the
  // given |field_types| do not correspond to a LabelFormatter, then nullptr
  // will be returned.
  static std::unique_ptr<LabelFormatter> Create(
      const std::string& app_locale,
      ServerFieldType focused_field_type,
      const std::vector<ServerFieldType>& field_types);

 protected:
  const std::string& app_locale() const { return app_locale_; }
  ServerFieldType focused_field_type() const { return focused_field_type_; }
  const std::vector<ServerFieldType>& field_types_for_labels() const {
    return field_types_for_labels_;
  }
  // Returns the FieldTypeGroup with which |focused_field_type_| is associated.
  // Billing field types are mapped to their corresponding home address field
  // types. For example, if |focused_field_type_| is ADDRESS_BILLING_ZIP, then
  // the resulting FieldTypeGroup is ADDRESS_HOME instead of ADDRESS_BILLING.
  FieldTypeGroup GetFocusedGroup() const;

 private:
  // The locale for which to generate labels. This reflects the language and
  // country for which the application is translated, e.g. en_AU for Austalian
  // English.
  std::string app_locale_;

  // The type of field on which the user is focused, e.g. NAME_FIRST.
  ServerFieldType focused_field_type_;

  // A collection of field types that can be used to make labels. It includes
  // types related to names, addresses, email addresses, and phone numbers.
  // It excludes types related to countries and to focused_group_.
  std::vector<ServerFieldType> field_types_for_labels_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
