// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
#define CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_

#include <vector>

#include "base/string16.h"

// This class is an interface for collections of form fields, grouped by type.
// The information in objects of this class is managed by the
// PersonalDataManager.
class FormGroup {
 public:
  virtual ~FormGroup() {}

  // Returns a clone of this object.
  virtual FormGroup* Clone() const = 0;

  // Used to determine the type of a field based on the text that a user enters
  // into the field. The field types can then be reported back to the server.
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const = 0;

  // Returns the string that should be autofilled into a text field given the
  // type of that field.
  virtual string16 GetFieldText(const AutoFillType& type) const = 0;

  // Returns the text for preview.
  virtual string16 GetPreviewText(const AutoFillType& type) const {
    return GetFieldText(type);
  }

  // Used to determine if the text being typed into a field matches the
  // information in this FormGroup object. This is used by the preview
  // functionality.  |matched_text| will be populated with all of the possible
  // matches given the type.
  virtual void FindInfoMatches(const AutoFillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const = 0;

  // Used to populate this FormGroup object with data.
  virtual void SetInfo(const AutoFillType& type, const string16& value) = 0;

  // Returns the label for this FormGroup item. This should be overridden for
  // form group items that implement a label.
  virtual string16 Label() const { return EmptyString(); }
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
