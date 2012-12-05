// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_

#include <stddef.h>

#include <iosfwd>
#include <list>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/contact_info.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/autofill/phone_number.h"

struct FormFieldData;

// A collection of FormGroups stored in a profile.  AutofillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutofillProfile : public FormGroup {
 public:
  explicit AutofillProfile(const std::string& guid);

  // For use in STL containers.
  AutofillProfile();
  AutofillProfile(const AutofillProfile& profile);
  virtual ~AutofillProfile();

  AutofillProfile& operator=(const AutofillProfile& profile);

  // FormGroup:
  virtual std::string GetGUID() const OVERRIDE;
  virtual void GetMatchingTypes(const string16& text,
                                const std::string& app_locale,
                                FieldTypeSet* matching_types) const OVERRIDE;
  virtual string16 GetRawInfo(AutofillFieldType type) const OVERRIDE;
  virtual void SetRawInfo(AutofillFieldType type,
                          const string16& value) OVERRIDE;
  virtual string16 GetInfo(AutofillFieldType type,
                           const std::string& app_locale) const OVERRIDE;
  virtual bool SetInfo(AutofillFieldType type,
                       const string16& value,
                       const std::string& app_locale) OVERRIDE;
  virtual void FillFormField(const AutofillField& field,
                             size_t variant,
                             FormFieldData* field_data) const OVERRIDE;

  // Multi-value equivalents to |GetInfo| and |SetInfo|.
  void SetRawMultiInfo(AutofillFieldType type,
                       const std::vector<string16>& values);
  void GetRawMultiInfo(AutofillFieldType type,
                       std::vector<string16>* values) const;
  void GetMultiInfo(AutofillFieldType type,
                    const std::string& app_locale,
                    std::vector<string16>* values) const;

  // Set |field_data|'s value for phone number based on contents of |this|.
  // The |field| specifies the type of the phone and whether this is a
  // phone prefix or suffix.  The |variant| parameter specifies which value in a
  // multi-valued profile.
  void FillPhoneNumberField(const AutofillField& field,
                            size_t variant,
                            FormFieldData* field_data) const;

  // The user-visible label of the profile, generated in relation to other
  // profiles. Shows at least 2 fields that differentiate profile from other
  // profiles. See AdjustInferredLabels() further down for more description.
  const string16 Label() const;

  // This guid is the primary identifier for |AutofillProfile| objects.
  // TODO(estade): remove this and just use GetGUID(). |guid_| can probably
  // be moved to FormGroup.
  const std::string guid() const { return guid_; }
  void set_guid(const std::string& guid) { guid_ = guid; }

  // Accessors for the stored address's country code.
  const std::string CountryCode() const;
  void SetCountryCode(const std::string& country_code);

  // Returns true if there are no values (field types) set.
  bool IsEmpty() const;

  // Comparison for Sync.  Returns 0 if the profile is the same as |this|,
  // or < 0, or > 0 if it is different.  The implied ordering can be used for
  // culling duplicates.  The ordering is based on collation order of the
  // textual contents of the fields.
  // GUIDs are not compared, only the values of the contents themselves.
  // Full profile comparision, comparison includes multi-valued fields.
  int Compare(const AutofillProfile& profile) const;

  // Equality operators compare GUIDs and the contents in the comparison.
  bool operator==(const AutofillProfile& profile) const;
  virtual bool operator!=(const AutofillProfile& profile) const;

  // Returns concatenation of full name and address line 1.  This acts as the
  // basis of comparison for new values that are submitted through forms to
  // aid with correct aggregation of new data.
  const string16 PrimaryValue() const;

  // Returns true if the data in this AutofillProfile is a subset of the data in
  // |profile|.
  bool IsSubsetOf(const AutofillProfile& profile) const;

  // Overwrites the single-valued field data in |profile| with this
  // Profile.  Or, for multi-valued fields append the new values.
  void OverwriteWithOrAddTo(const AutofillProfile& profile);

  // Returns |true| if |type| accepts multi-values.
  static bool SupportsMultiValue(AutofillFieldType type);

  // Adjusts the labels according to profile data.
  // Labels contain minimal different combination of:
  // 1. Full name.
  // 2. Address.
  // 3. E-mail.
  // 4. Phone.
  // 5. Company name.
  // Profile labels are changed accordingly to these rules.
  // Returns true if any of the profiles were updated.
  // This function is useful if you want to adjust unique labels for all
  // profiles. For non permanent situations (selection of profile, when user
  // started typing in the field, for example) use CreateInferredLabels().
  static bool AdjustInferredLabels(std::vector<AutofillProfile*>* profiles);

  // Creates inferred labels for |profiles|, according to the rules above and
  // stores them in |created_labels|. If |suggested_fields| is not NULL, the
  // resulting label fields are drawn from |suggested_fields|, except excluding
  // |excluded_field|. Otherwise, the label fields are drawn from a default set,
  // and |excluded_field| is ignored; by convention, it should be of
  // |UNKNOWN_TYPE| when |suggested_fields| is NULL. Each label includes at
  // least |minimal_fields_shown| fields, if possible.
  static void CreateInferredLabels(
      const std::vector<AutofillProfile*>* profiles,
      const std::vector<AutofillFieldType>* suggested_fields,
      AutofillFieldType excluded_field,
      size_t minimal_fields_shown,
      std::vector<string16>* created_labels);

 private:
  typedef std::vector<const FormGroup*> FormGroupList;

  // FormGroup:
  virtual bool FillCountrySelectControl(FormFieldData* field) const OVERRIDE;
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const OVERRIDE;

  // Shared implementation for GetRawMultiInfo() and GetMultiInfo().  Pass an
  // empty |app_locale| to get the raw info; otherwise, the returned info is
  // canonicalized according to the given |app_locale|, if appropriate.
  void GetMultiInfoImpl(AutofillFieldType type,
                        const std::string& app_locale,
                        std::vector<string16>* values) const;

  // Checks if the |phone| is in the |existing_phones| using fuzzy matching:
  // for example, "1-800-FLOWERS", "18003569377", "(800)356-9377" and "356-9377"
  // are considered the same.
  // Adds the |phone| to the |existing_phones| if not already there.
  void AddPhoneIfUnique(const string16& phone,
                        std::vector<string16>* existing_phones);

  // Builds inferred label from the first |num_fields_to_include| non-empty
  // fields in |label_fields|. Uses as many fields as possible if there are not
  // enough non-empty fields.
  string16 ConstructInferredLabel(
      const std::vector<AutofillFieldType>& label_fields,
      size_t num_fields_to_include) const;

  // Creates inferred labels for |profiles| at indices corresponding to
  // |indices|, and stores the results to the corresponding elements of
  // |created_labels|. These labels include enough fields to differentiate among
  // the profiles, if possible; and also at least |num_fields_to_include|
  // fields, if possible. The label fields are drawn from |fields|.
  static void CreateDifferentiatingLabels(
      const std::vector<AutofillProfile*>& profiles,
      const std::list<size_t>& indices,
      const std::vector<AutofillFieldType>& fields,
      size_t num_fields_to_include,
      std::vector<string16>* created_labels);

  // Utilities for listing and lookup of the data members that constitute
  // user-visible profile information.
  FormGroupList FormGroups() const;
  const FormGroup* FormGroupForType(AutofillFieldType type) const;
  FormGroup* MutableFormGroupForType(AutofillFieldType type);

  // The label presented to the user when selecting a profile.
  string16 label_;

  // The guid of this profile.
  std::string guid_;

  // Personal information for this profile.
  std::vector<NameInfo> name_;
  std::vector<EmailInfo> email_;
  CompanyInfo company_;
  std::vector<PhoneNumber> home_number_;
  Address address_;
};

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile);

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_PROFILE_H_
