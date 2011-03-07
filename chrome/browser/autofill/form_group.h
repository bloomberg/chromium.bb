// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
#define CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

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
  // This method is additive on |possible_types|.
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const = 0;

  // Returns a set of AutofillFieldTypes for which this FormGroup has non-empty
  // data.
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const = 0;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field.
  virtual string16 GetFieldText(const AutofillType& type) const = 0;

  // Returns the text for preview.
  virtual string16 GetPreviewText(const AutofillType& type) const;

  // Used to determine if the text being typed into a field matches the
  // information in this FormGroup object. This is used by the preview
  // functionality.  |matched_text| will be populated with all of the possible
  // matches given the type.  This method is additive on |matched_text|.
  virtual void FindInfoMatches(const AutofillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const = 0;

  // Used to populate this FormGroup object with data.
  virtual void SetInfo(const AutofillType& type, const string16& value) = 0;

  // Returns the label for this FormGroup item. This should be overridden for
  // form group items that implement a label.
  virtual const string16 Label() const;

  // Returns true if the field data in |form_group| does not match the field
  // data in this FormGroup.
  virtual bool operator!=(const FormGroup& form_group) const;

  // Returns true if the data in this FormGroup is a subset of the data in
  // |form_group|.
  bool IsSubsetOf(const FormGroup& form_group) const;

  // Returns true if the values of the intersection of the available field types
  // are equal.  If the intersection is empty, the method returns false.
  bool IntersectionOfTypesHasEqualValues(const FormGroup& form_group) const;

  // Merges the field data in |form_group| with this FormGroup.
  void MergeWith(const FormGroup& form_group);

  // Overwrites the field data in |form_group| with this FormGroup.
  void OverwriteWith(const FormGroup& form_group);
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_GROUP_H_
