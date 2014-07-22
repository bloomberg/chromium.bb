// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_H_

#include <stddef.h>

#include <iosfwd>
#include <list>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/address.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/contact_info.h"
#include "components/autofill/core/browser/phone_number.h"

namespace autofill {

struct FormFieldData;

// A collection of FormGroups stored in a profile.  AutofillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutofillProfile : public AutofillDataModel {
 public:
  AutofillProfile(const std::string& guid, const std::string& origin);

  // For use in STL containers.
  AutofillProfile();
  AutofillProfile(const AutofillProfile& profile);
  virtual ~AutofillProfile();

  AutofillProfile& operator=(const AutofillProfile& profile);

  // FormGroup:
  virtual void GetMatchingTypes(
      const base::string16& text,
      const std::string& app_locale,
      ServerFieldTypeSet* matching_types) const OVERRIDE;
  virtual base::string16 GetRawInfo(ServerFieldType type) const OVERRIDE;
  virtual void SetRawInfo(ServerFieldType type,
                          const base::string16& value) OVERRIDE;
  virtual base::string16 GetInfo(const AutofillType& type,
                                 const std::string& app_locale) const OVERRIDE;
  virtual bool SetInfo(const AutofillType& type,
                       const base::string16& value,
                       const std::string& app_locale) OVERRIDE;

  // AutofillDataModel:
  virtual base::string16 GetInfoForVariant(
      const AutofillType& type,
      size_t variant,
      const std::string& app_locale) const OVERRIDE;

  // Multi-value equivalents to |GetInfo| and |SetInfo|.
  void SetRawMultiInfo(ServerFieldType type,
                       const std::vector<base::string16>& values);
  void GetRawMultiInfo(ServerFieldType type,
                       std::vector<base::string16>* values) const;
  void SetMultiInfo(const AutofillType& type,
                    const std::vector<base::string16>& values,
                    const std::string& app_locale);
  void GetMultiInfo(const AutofillType& type,
                    const std::string& app_locale,
                    std::vector<base::string16>* values) const;

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns true if the |type| of data in this profile is present, but invalid.
  // Otherwise returns false.
  bool IsPresentButInvalid(ServerFieldType type) const;

  // Comparison for Sync.  Returns 0 if the profile is the same as |this|,
  // or < 0, or > 0 if it is different.  The implied ordering can be used for
  // culling duplicates.  The ordering is based on collation order of the
  // textual contents of the fields. Full profile comparison, comparison
  // includes multi-valued fields.
  //
  // GUIDs, origins, and language codes are not compared, only the contents
  // themselves.
  int Compare(const AutofillProfile& profile) const;

  // Same as operator==, but ignores differences in origin.
  bool EqualsSansOrigin(const AutofillProfile& profile) const;

  // Same as operator==, but ignores differences in GUID.
  bool EqualsSansGuid(const AutofillProfile& profile) const;

  // Equality operators compare GUIDs, origins, language code, and the contents
  // in the comparison.
  bool operator==(const AutofillProfile& profile) const;
  virtual bool operator!=(const AutofillProfile& profile) const;

  // Returns concatenation of full name and address line 1.  This acts as the
  // basis of comparison for new values that are submitted through forms to
  // aid with correct aggregation of new data.
  const base::string16 PrimaryValue() const;

  // Returns true if the data in this AutofillProfile is a subset of the data in
  // |profile|.
  bool IsSubsetOf(const AutofillProfile& profile,
                  const std::string& app_locale) const;

  // Overwrites the single-valued field data in |profile| with this
  // Profile.  Or, for multi-valued fields append the new values.
  void OverwriteWithOrAddTo(const AutofillProfile& profile,
                            const std::string& app_locale);

  // Returns |true| if |type| accepts multi-values.
  static bool SupportsMultiValue(ServerFieldType type);

  // Creates a differentiating label for each of the |profiles|.
  // Labels consist of the minimal differentiating combination of:
  // 1. Full name.
  // 2. Address.
  // 3. E-mail.
  // 4. Phone.
  // 5. Company name.
  static void CreateDifferentiatingLabels(
      const std::vector<AutofillProfile*>& profiles,
      const std::string& app_locale,
      std::vector<base::string16>* labels);

  // Creates inferred labels for |profiles|, according to the rules above and
  // stores them in |created_labels|. If |suggested_fields| is not NULL, the
  // resulting label fields are drawn from |suggested_fields|, except excluding
  // |excluded_field|. Otherwise, the label fields are drawn from a default set,
  // and |excluded_field| is ignored; by convention, it should be of
  // |UNKNOWN_TYPE| when |suggested_fields| is NULL. Each label includes at
  // least |minimal_fields_shown| fields, if possible.
  static void CreateInferredLabels(
      const std::vector<AutofillProfile*>& profiles,
      const std::vector<ServerFieldType>* suggested_fields,
      ServerFieldType excluded_field,
      size_t minimal_fields_shown,
      const std::string& app_locale,
      std::vector<base::string16>* labels);

  const std::string& language_code() const { return language_code_; }
  void set_language_code(const std::string& language_code) {
    language_code_ = language_code;
  }

 private:
  typedef std::vector<const FormGroup*> FormGroupList;

  // FormGroup:
  virtual void GetSupportedTypes(
      ServerFieldTypeSet* supported_types) const OVERRIDE;

  // Shared implementation for GetRawMultiInfo() and GetMultiInfo().  Pass an
  // empty |app_locale| to get the raw info; otherwise, the returned info is
  // canonicalized according to the given |app_locale|, if appropriate.
  void GetMultiInfoImpl(const AutofillType& type,
                        const std::string& app_locale,
                        std::vector<base::string16>* values) const;

  // Builds inferred label from the first |num_fields_to_include| non-empty
  // fields in |label_fields|. Uses as many fields as possible if there are not
  // enough non-empty fields.
  base::string16 ConstructInferredLabel(
      const std::vector<ServerFieldType>& label_fields,
      size_t num_fields_to_include,
      const std::string& app_locale) const;

  // Creates inferred labels for |profiles| at indices corresponding to
  // |indices|, and stores the results to the corresponding elements of
  // |labels|. These labels include enough fields to differentiate among the
  // profiles, if possible; and also at least |num_fields_to_include| fields, if
  // possible. The label fields are drawn from |fields|.
  static void CreateInferredLabelsHelper(
      const std::vector<AutofillProfile*>& profiles,
      const std::list<size_t>& indices,
      const std::vector<ServerFieldType>& fields,
      size_t num_fields_to_include,
      const std::string& app_locale,
      std::vector<base::string16>* labels);

  // Utilities for listing and lookup of the data members that constitute
  // user-visible profile information.
  FormGroupList FormGroups() const;
  const FormGroup* FormGroupForType(const AutofillType& type) const;
  FormGroup* MutableFormGroupForType(const AutofillType& type);

  // Appends unique names from |names| onto the |name_| list, dropping
  // duplicates. If a name in |names| has the same full name representation
  // as a name in |name_|, keeps the variant that has more information (i.e.
  // is not reconstructible via a heuristic parse of the full name string).
  void OverwriteOrAppendNames(const std::vector<NameInfo>& names,
                              const std::string& app_locale);

  // Personal information for this profile.
  std::vector<NameInfo> name_;
  std::vector<EmailInfo> email_;
  CompanyInfo company_;
  std::vector<PhoneNumber> phone_number_;
  Address address_;

  // The BCP 47 language code that can be used to format |address_| for display.
  std::string language_code_;
};

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_H_
