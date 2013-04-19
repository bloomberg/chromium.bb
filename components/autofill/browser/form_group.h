// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_FORM_GROUP_H_
#define COMPONENTS_AUTOFILL_BROWSER_FORM_GROUP_H_

#include <string>

#include "base/string16.h"
#include "components/autofill/browser/field_types.h"

namespace autofill {

// This class is an interface for collections of form fields, grouped by type.
class FormGroup {
 public:
  virtual ~FormGroup() {}

  // Used to determine the type of a field based on the text that a user enters
  // into the field, interpreted in the given |app_locale| if appropriate.  The
  // field types can then be reported back to the server.  This method is
  // additive on |matching_types|.
  virtual void GetMatchingTypes(const base::string16& text,
                                const std::string& app_locale,
                                FieldTypeSet* matching_types) const;

  // Returns a set of AutofillFieldTypes for which this FormGroup has non-empty
  // data.  This method is additive on |non_empty_types|.
  virtual void GetNonEmptyTypes(const std::string& app_locale,
                                FieldTypeSet* non_empty_types) const;

  // Returns the string associated with |type|, without canonicalizing the
  // returned value.  For user-visible strings, use GetInfo() instead.
  virtual base::string16 GetRawInfo(AutofillFieldType type) const = 0;

  // Sets this FormGroup object's data for |type| to |value|, without
  // canonicalizing the |value|.  For data that has not already been
  // canonicalized, use SetInfo() instead.
  virtual void SetRawInfo(AutofillFieldType type,
                          const base::string16& value) = 0;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given |app_locale| if appropriate.
  virtual base::string16 GetInfo(AutofillFieldType type,
                                 const std::string& app_locale) const;

  // Used to populate this FormGroup object with data.  Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  virtual bool SetInfo(AutofillFieldType type,
                       const base::string16& value,
                       const std::string& app_locale);

 protected:
  // AutofillProfile needs to call into GetSupportedTypes() for objects of
  // non-AutofillProfile type, for which mere inheritance is insufficient.
  friend class AutofillProfile;

  // Returns a set of AutofillFieldTypes for which this FormGroup can store
  // data.  This method is additive on |supported_types|.
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_FORM_GROUP_H_
