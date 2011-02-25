// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
#pragma once

#include <list>
#include <map>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

typedef std::map<FieldTypeGroup, FormGroup*> FormGroupMap;

// A collection of FormGroups stored in a profile.  AutoFillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutoFillProfile : public FormGroup {
 public:
  explicit AutoFillProfile(const std::string& guid);

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
  // The user-visible label of the profile, generated in relation to other
  // profiles. Shows at least 2 fields that differentiate profile from other
  // profiles. See AdjustInferredLabels() further down for more description.
  virtual const string16 Label() const;

  // This guid is the primary identifier for |AutoFillProfile| objects.
  const std::string guid() const { return guid_; }
  void set_guid(const std::string& guid) { guid_ = guid; }

  // Accessors for the stored address's country code.
  const std::string CountryCode() const;
  void SetCountryCode(const std::string& country_code);

  // Adjusts the labels according to profile data.
  // Labels contain minimal different combination of:
  // 1. Full name.
  // 2. Address.
  // 3. E-mail.
  // 4. Phone.
  // 5. Fax.
  // 6. Company name.
  // Profile labels are changed accordingly to these rules.
  // Returns true if any of the profiles were updated.
  // This function is useful if you want to adjust unique labels for all
  // profiles. For non permanent situations (selection of profile, when user
  // started typing in the field, for example) use CreateInferredLabels().
  static bool AdjustInferredLabels(std::vector<AutoFillProfile*>* profiles);

  // Creates inferred labels for |profiles|, according to the rules above and
  // stores them in |created_labels|. If |suggested_fields| is not NULL, the
  // resulting label fields are drawn from |suggested_fields|, except excluding
  // |excluded_field|. Otherwise, the label fields are drawn from a default set,
  // and |excluded_field| is ignored; by convention, it should be of
  // |UNKNOWN_TYPE| when |suggested_fields| is NULL. Each label includes at
  // least |minimal_fields_shown| fields, if possible.
  static void CreateInferredLabels(
      const std::vector<AutoFillProfile*>* profiles,
      const std::vector<AutoFillFieldType>* suggested_fields,
      AutoFillFieldType excluded_field,
      size_t minimal_fields_shown,
      std::vector<string16>* created_labels);

  // Returns true if there are no values (field types) set.
  bool IsEmpty() const;

  // For use in STL containers.
  void operator=(const AutoFillProfile&);

  // Comparison for Sync.  Returns 0 if the profile is the same as |this|,
  // or < 0, or > 0 if it is different.  The implied ordering can be used for
  // culling duplicates.  The ordering is based on collation order of the
  // textual contents of the fields.
  // GUIDs, labels, and unique IDs are not compared, only the values of the
  // profiles themselves.
  int Compare(const AutoFillProfile& profile) const;

  // TODO(dhollowa): These operators need to be made private and then the unit
  // tests that use them made friends.  The public |Compare| method should be
  // used by external clients (such as Sync).
  // http://crbug.com/58813
  bool operator==(const AutoFillProfile& profile) const;
  virtual bool operator!=(const AutoFillProfile& profile) const;

  // Returns concatenation of full name and address line 1.  This acts as the
  // basis of comparison for new values that are submitted through forms to
  // aid with correct aggregation of new data.
  const string16 PrimaryValue() const;

 private:
  // Builds inferred label from the first |num_fields_to_include| non-empty
  // fields in |label_fields|. Uses as many fields as possible if there are not
  // enough non-empty fields.
  string16 ConstructInferredLabel(
      const std::vector<AutoFillFieldType>& label_fields,
      size_t num_fields_to_include) const;

  // Creates inferred labels for |profiles| at indices corresponding to
  // |indices|, and stores the results to the corresponding elements of
  // |created_labels|. These labels include enough fields to differentiate among
  // the profiles, if possible; and also at least |num_fields_to_include|
  // fields, if possible. The label fields are drawn from |fields|.
  static void CreateDifferentiatingLabels(
      const std::vector<AutoFillProfile*>& profiles,
      const std::list<size_t>& indices,
      const std::vector<AutoFillFieldType>& fields,
      size_t num_fields_to_include,
      std::vector<string16>* created_labels);

  void set_label(const string16& label) { label_ = label; }

  // The label presented to the user when selecting a profile.
  string16 label_;

  // The guid of this profile.
  std::string guid_;

  // Personal information for this profile.
  FormGroupMap personal_info_;
};

// So we can compare AutoFillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutoFillProfile& profile);

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
