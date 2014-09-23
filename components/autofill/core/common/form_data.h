// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_

#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/gurl.h"

namespace autofill {

// Holds information about a form to be filled and/or submitted.
struct FormData {
  FormData();
  FormData(const FormData& data);
  ~FormData();

  // Used in testing, and in recording metrics and setting preferences, where
  // false positives/negatives aren't super important.
  bool operator==(const FormData& form) const;
  bool operator!=(const FormData& form) const;

  // Allow FormData to be a key in STL containers.
  bool operator<(const FormData& form) const;

  // The name of the form.
  base::string16 name;
  // The URL (minus query parameters) containing the form.
  GURL origin;
  // The action target of the form.
  GURL action;
  // true if this form was submitted by a user gesture and not javascript.
  bool user_submitted;
  // A vector of all the input fields in the form.
  std::vector<FormFieldData> fields;
};

// For testing.
std::ostream& operator<<(std::ostream& os, const FormData& form);

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms. Serialized data is appended to |pickle|
void SerializeFormData(const FormData& form_data, Pickle* pickle);
// Deserialize FormData. This assumes that |iter| is currently pointing to
// the part of a pickle created by SerializeFormData. Returns true on success.
bool DeserializeFormData(PickleIterator* iter, FormData* form_data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
