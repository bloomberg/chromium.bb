// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
#define CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_

#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/field_types.h"

struct FormFieldData;

// This class is an interface for collections of form fields, grouped by type.
// The information in objects of this class is managed by the
// PersonalDataManager.
class FormGroup {
 public:
  virtual ~FormGroup() {}

  // Used to determine the type of a field based on the text that a user enters
  // into the field.  The field types can then be reported back to the server.
  // This method is additive on |matching_types|.
  virtual void GetMatchingTypes(const string16& text,
                                FieldTypeSet* matching_types) const;

  // Returns a set of AutofillFieldTypes for which this FormGroup has non-empty
  // data.  This method is additive on |non_empty_types|.
  virtual void GetNonEmptyTypes(FieldTypeSet* non_empty_types) const;

  // Returns the string associated with |type|, without canonicalizing the
  // returned value.  For user-visible strings, use GetCanonicalizedInfo()
  // instead.
  virtual string16 GetRawInfo(AutofillFieldType type) const = 0;

  // Sets this FormGroup object's data for |type| to |value|, without
  // canonicalizing the |value|.  For data that has not already been
  // canonicalized, use SetCanonicalizedInfo() instead.
  virtual void SetRawInfo(AutofillFieldType type, const string16& value) = 0;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field.
  virtual string16 GetCanonicalizedInfo(AutofillFieldType type) const;

  // Used to populate this FormGroup object with data.  Canonicalizes the data
  // prior to storing, if appropriate.
  virtual bool SetCanonicalizedInfo(AutofillFieldType type,
                                    const string16& value);

  // Fills in select control with data matching |type| from |this|.
  // Public for testing purposes.
  void FillSelectControl(AutofillFieldType type,
                         FormFieldData* field_data) const;

  // Returns true if |value| is a valid US state name or abbreviation.  It is
  // case insensitive.  Valid for US states only.
  // TODO(estade): this is a crappy place for this function.
  static bool IsValidState(const string16& value);

 protected:
  // AutofillProfile needs to call into GetSupportedTypes() for objects of
  // non-AutofillProfile type, for which mere inheritance is insufficient.
  friend class AutofillProfile;

  // Returns a set of AutofillFieldTypes for which this FormGroup can store
  // data.  This method is additive on |supported_types|.
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const = 0;

  // Fills in a select control for a country from data in |this|. Returns true
  // for success.
  virtual bool FillCountrySelectControl(FormFieldData* field_data) const;
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
