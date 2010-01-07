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
  virtual ~AutoFillProfile();

  // FormGroup implementation:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
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
  virtual string16 Label() const { return label_; }

  // NOTE: callers must write the profile to the WebDB after changing the value
  // of use_billing_address.
  void set_use_billing_address(bool use);
  bool use_billing_address() const { return use_billing_address_; }

  int unique_id() const { return unique_id_; }

  // TODO(jhawkins): Implement RemoveProfile.

 private:
  // This constructor should only be used by the copy constructor.
  AutoFillProfile() {}

  Address* GetBillingAddress();
  Address* GetHomeAddress();

  // The label presented to the user when selecting a profile.
  string16 label_;

  // The unique ID of this profile.
  int unique_id_;

  // If true, the billing address will be used for the home address.  Correlates
  // with the "Use billing address" option on some billing forms.
  bool use_billing_address_;

  // Personal information for this profile.
  FormGroupMap personal_info_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillProfile);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
