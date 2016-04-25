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

// A collection of FormGroups stored in a profile.  AutofillProfile also
// implements the FormGroup interface so that owners of this object can request
// form information from the profile, and the profile will delegate the request
// to the requested form group type.
class AutofillProfile : public AutofillDataModel {
 public:
  enum RecordType {
    // A profile stored and editable locally.
    LOCAL_PROFILE,

    // A profile synced down from the server. These are read-only locally.
    SERVER_PROFILE,

    // An auxiliary profile, such as a Mac address book entry.
    AUXILIARY_PROFILE,
  };

  AutofillProfile(const std::string& guid, const std::string& origin);

  // Server profile constructor. The type must be SERVER_PROFILE (this serves
  // to differentiate this constructor). |server_id| can be empty. If empty,
  // callers should invoke GenerateServerProfileIdentifier after setting data.
  AutofillProfile(RecordType type, const std::string& server_id);

  // For use in STL containers.
  AutofillProfile();
  AutofillProfile(const AutofillProfile& profile);
  ~AutofillProfile() override;

  AutofillProfile& operator=(const AutofillProfile& profile);

  // FormGroup:
  void GetMatchingTypes(const base::string16& text,
                        const std::string& app_locale,
                        ServerFieldTypeSet* matching_types) const override;
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;
  base::string16 GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  bool SetInfo(const AutofillType& type,
               const base::string16& value,
               const std::string& app_locale) override;

  // How this card is stored.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType type) { record_type_ = type; }

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

  // Same as operator==, but ignores differences in guid and cares about
  // differences in usage stats.
  bool EqualsForSyncPurposes(const AutofillProfile& profile) const;

  // Equality operators compare GUIDs, origins, language code, and the contents
  // in the comparison. Usage metadata (use count, use date, modification date)
  // are NOT compared.
  bool operator==(const AutofillProfile& profile) const;
  virtual bool operator!=(const AutofillProfile& profile) const;

  // Returns concatenation of first name, last name, address line 1 and city.
  // This acts as the basis of comparison for new values that are submitted
  // through forms to aid with correct aggregation of new data.
  const base::string16 PrimaryValue() const;

  // Returns true if the data in this AutofillProfile is a subset of the data in
  // |profile|.
  bool IsSubsetOf(const AutofillProfile& profile,
                  const std::string& app_locale) const;

  // Like IsSubsetOf, but only considers the types present in |types|.
  bool IsSubsetOfForFieldSet(const AutofillProfile& profile,
                             const std::string& app_locale,
                             const ServerFieldTypeSet& types) const;

  // Overwrites the field data in |profile| with this Profile. Returns |true| if
  // at least one field was overwritten.
  bool OverwriteWith(const AutofillProfile& profile,
                     const std::string& app_locale);

  // Saves info from |profile| into |this|, provided |this| and |profile| do not
  // have any direct conflicts (i.e. data is present but different). Will not
  // make changes if |this| is verified and |profile| is not. Returns true if
  // |this| and |profile| are similar.
  bool SaveAdditionalInfo(const AutofillProfile& profile,
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

  // Nonempty only when type() == SERVER_PROFILE. base::kSHA1Length bytes long.
  // Not necessarily valid UTF-8.
  const std::string& server_id() const { return server_id_; }

  // Creates an identifier and saves it as |server_id_|. Only used for
  // server credit cards. The server doesn't attach an identifier so Chrome
  // creates its own. The ID is a hash of the data contained in the profile.
  void GenerateServerProfileIdentifier();

  // Logs the number of days since the profile was last used and records its
  // use.
  void RecordAndLogUse();

  // TODO(crbug.com/574081): Move common profile methods to a utils file.
  // Returns a standardized representation of the given string for comparison
  // purposes. The resulting string will be lower-cased with all punctuation
  // substituted by spaces. Whitespace will be converted to ASCII space, and
  // multiple whitespace characters will be collapsed.
  //
  // This string is designed for comparison purposes only and isn't suitable
  // for storing or displaying to the user.
  static base::string16 CanonicalizeProfileString(const base::string16& str);

  // Returns true if the given two profile strings are similar enough that
  // they probably refer to the same thing.
  static bool AreProfileStringsSimilar(const base::string16& a,
                                       const base::string16& b);

 private:
  typedef std::vector<const FormGroup*> FormGroupList;

  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

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

  // If |name| has the same full name representation as |name_|,
  // this will keep the one that has more information (i.e.
  // is not reconstructible via a heuristic parse of the full name string).
  // Returns |true| is |name_| was overwritten.
  bool OverwriteName(const NameInfo& name, const std::string& app_locale);

  // Same as operator==, but ignores differences in GUID.
  bool EqualsSansGuid(const AutofillProfile& profile) const;

  RecordType record_type_;

  // Personal information for this profile.
  NameInfo name_;
  EmailInfo email_;
  CompanyInfo company_;
  PhoneNumber phone_number_;
  Address address_;

  // The BCP 47 language code that can be used to format |address_| for display.
  std::string language_code_;

  // ID used for identifying this profile. Only set for SERVER_PROFILEs. This is
  // a hash of the contents.
  std::string server_id_;
};

// So we can compare AutofillProfiles with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_H_
