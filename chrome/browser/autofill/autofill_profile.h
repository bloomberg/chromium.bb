// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_

#include <map>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

class Address;
typedef std::map<FieldTypeGroup, FormGroup*> FormGroupMap;

// A collection of FormGroups stored in a profile.  AutoFillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutoFillProfile : public FormGroup {
 public:
  AutoFillProfile(const string16& label, int unique_id);
  // For use in STL containers.
  AutoFillProfile();
  AutoFillProfile(const AutoFillProfile&);
  virtual ~AutoFillProfile();

  // FormGroup implementation:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual string16 GetFieldText(const AutoFillType& type) const;
  // Returns true if the info matches the profile data corresponding to type.
  // If the type is UNKNOWN_TYPE then info will be matched against all of the
  // profile data.
  virtual void FindInfoMatches(const AutoFillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual void SetInfo(const AutoFillType& type, const string16& value);
  // Returns a copy of the profile it is called on. The caller is responsible
  // for deleting profile when they are done with it.
  virtual FormGroup* Clone() const;
  virtual const string16& Label() const { return label_; }

  void set_unique_id(int id) { unique_id_ = id; }
  int unique_id() const { return unique_id_; }

  // Profile summary string for UI.
  // Constructs a summary string based on NAME_FIRST, NAME_LAST, and
  // ADDRESS_HOME_LINE1 fields of the profile.  The summary string is of the
  // form:
  //     L"<first_name> <last_name>, <address_line_1>"
  // but may omit any or all of the fields if they are not present in the
  // profile.
  // The form of the string is governed by generated resources.
  string16 PreviewSummary() const;

  // For use in STL containers.
  void operator=(const AutoFillProfile&);

  // For WebData and Sync.
  bool operator==(const AutoFillProfile& profile) const;
  virtual bool operator!=(const AutoFillProfile& profile) const;
  void set_label(const string16& label) { label_ = label; }

 private:
  Address* GetHomeAddress();

  // The label presented to the user when selecting a profile.
  string16 label_;

  // The unique ID of this profile.
  int unique_id_;

  // Personal information for this profile.
  FormGroupMap personal_info_;
};

// So we can compare AutoFillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutoFillProfile& profile);

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
