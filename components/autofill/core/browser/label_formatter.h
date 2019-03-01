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
  virtual ~LabelFormatter() = default;
  // Returns a collection of |labels| formed by extracting useful disambiguating
  // information from a collection of |profiles|.
  virtual std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_H_
