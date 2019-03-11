// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_

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
                 FieldTypeGroup focused_group,
                 const std::vector<ServerFieldType>& field_types);
  virtual ~LabelFormatter();

  // Returns a collection of |labels| formed by extracting useful disambiguating
  // information from a collection of |profiles|.
  virtual std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const = 0;

 protected:
  const std::string& app_locale() const { return app_locale_; }
  FieldTypeGroup focused_group() const { return focused_group_; }
  const std::vector<ServerFieldType>& field_types_for_labels() const {
    return field_types_for_labels_;
  }

 private:
  // The locale for which to generate labels. This reflects the language and
  // country for which the application is translated, e.g. en_AU for Austalian
  // English.
  std::string app_locale_;

  // The group of the field on which the user is focused. For example, NAME
  // is the group of the NAME_FIRST and NAME_MIDDLE fields.
  FieldTypeGroup focused_group_;

  // A collection of field types that can be used to make labels. It includes
  // types related to names, addresses, email addresses, and phone numbers.
  // It excludes types related to countries and to focused_group_.
  std::vector<ServerFieldType> field_types_for_labels_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
