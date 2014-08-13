// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include "base/i18n/timezone.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill-inl.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace autofill {
namespace {

using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

const base::string16::value_type kCreditCardPrefix[] = {'*', 0};

template<typename T>
class FormGroupMatchesByGUIDFunctor {
 public:
  explicit FormGroupMatchesByGUIDFunctor(const std::string& guid)
      : guid_(guid) {
  }

  bool operator()(const T& form_group) {
    return form_group.guid() == guid_;
  }

  bool operator()(const T* form_group) {
    return form_group->guid() == guid_;
  }

 private:
  const std::string guid_;
};

template<typename T, typename C>
typename C::const_iterator FindElementByGUID(const C& container,
                                             const std::string& guid) {
  return std::find_if(container.begin(),
                      container.end(),
                      FormGroupMatchesByGUIDFunctor<T>(guid));
}

template<typename T, typename C>
bool FindByGUID(const C& container, const std::string& guid) {
  return FindElementByGUID<T>(container, guid) != container.end();
}

template<typename T>
class IsEmptyFunctor {
 public:
  explicit IsEmptyFunctor(const std::string& app_locale)
      : app_locale_(app_locale) {
  }

  bool operator()(const T& form_group) {
    return form_group.IsEmpty(app_locale_);
  }

 private:
  const std::string app_locale_;
};

// Returns true if minimum requirements for import of a given |profile| have
// been met.  An address submitted via a form must have at least the fields
// required as determined by its country code.
// No verification of validity of the contents is preformed. This is an
// existence check only.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& app_locale) {
  // All countries require at least one address line.
  if (profile.GetRawInfo(ADDRESS_HOME_LINE1).empty())
    return false;

  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (country_code.empty())
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);

  AutofillCountry country(country_code, app_locale);

  if (country.requires_city() && profile.GetRawInfo(ADDRESS_HOME_CITY).empty())
    return false;

  if (country.requires_state() &&
      profile.GetRawInfo(ADDRESS_HOME_STATE).empty())
    return false;

  if (country.requires_zip() && profile.GetRawInfo(ADDRESS_HOME_ZIP).empty())
    return false;

  return true;
}

// Return true if the |field_type| and |value| are valid within the context
// of importing a form.
bool IsValidFieldTypeAndValue(const std::set<ServerFieldType>& types_seen,
                              ServerFieldType field_type,
                              const base::string16& value) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for PHONE_HOME_NUMBER however as both prefix and
  // suffix are stored against this type, and for EMAIL_ADDRESS because it is
  // common to see second 'confirm email address' fields on forms.
  if (types_seen.count(field_type) &&
      field_type != PHONE_HOME_NUMBER &&
      field_type != EMAIL_ADDRESS)
    return false;

  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value))
    return false;

  return true;
}

// A helper function for finding the maximum value in a string->int map.
static bool CompareVotes(const std::pair<std::string, int>& a,
                         const std::pair<std::string, int>& b) {
  return a.second < b.second;
}

}  // namespace

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : database_(NULL),
      is_data_loaded_(false),
      pending_profiles_query_(0),
      pending_creditcards_query_(0),
      app_locale_(app_locale),
      metric_logger_(new AutofillMetrics),
      pref_service_(NULL),
      is_off_the_record_(false),
      has_logged_profile_count_(false) {}

void PersonalDataManager::Init(scoped_refptr<AutofillWebDataService> database,
                               PrefService* pref_service,
                               bool is_off_the_record) {
  database_ = database;
  SetPrefService(pref_service);
  is_off_the_record_ = is_off_the_record;

  if (!is_off_the_record_)
    metric_logger_->LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  // WebDataService may not be available in tests.
  if (!database_.get())
    return;

  LoadProfiles();
  LoadCreditCards();

  database_->AddObserver(this);
}

PersonalDataManager::~PersonalDataManager() {
  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_creditcards_query_);

  if (database_.get())
    database_->RemoveObserver(this);
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_profiles_query_ || pending_creditcards_query_);

  if (!result) {
    // Error from the web database.
    if (h == pending_creditcards_query_)
      pending_creditcards_query_ = 0;
    else if (h == pending_profiles_query_)
      pending_profiles_query_ = 0;
    return;
  }

  switch (result->GetType()) {
    case AUTOFILL_PROFILES_RESULT:
      ReceiveLoadedProfiles(h, result);
      break;
    case AUTOFILL_CREDITCARDS_RESULT:
      ReceiveLoadedCreditCards(h, result);
      break;
    default:
      NOTREACHED();
  }

  // If both requests have responded, then all personal data is loaded.
  if (pending_profiles_query_ == 0 && pending_creditcards_query_ == 0) {
    is_data_loaded_ = true;
    NotifyPersonalDataChanged();
  }
}

void PersonalDataManager::AutofillMultipleChanged() {
  Refresh();
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool PersonalDataManager::ImportFormData(
    const FormStructure& form,
    scoped_ptr<CreditCard>* imported_credit_card) {
  scoped_ptr<AutofillProfile> imported_profile(new AutofillProfile);
  scoped_ptr<CreditCard> local_imported_credit_card(new CreditCard);

  const std::string origin = form.source_url().spec();
  imported_profile->set_origin(origin);
  local_imported_credit_card->set_origin(origin);

  // Parse the form and construct a profile based on the information that is
  // possible to import.
  int importable_credit_card_fields = 0;

  // Detect and discard forms with multiple fields of the same type.
  // TODO(isherman): Some types are overlapping but not equal, e.g. phone number
  // parts, address parts.
  std::set<ServerFieldType> types_seen;

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper home;

  for (size_t i = 0; i < form.field_count(); ++i) {
    const AutofillField* field = form.field(i);
    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty())
      continue;

    AutofillType field_type = field->Type();
    ServerFieldType server_field_type = field_type.GetStorableType();
    FieldTypeGroup group(field_type.group());

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    if (server_field_type == EMAIL_ADDRESS) {
      if (types_seen.count(server_field_type) &&
          imported_profile->GetRawInfo(EMAIL_ADDRESS) != value) {
        imported_profile.reset();
        break;
      }
    }

    // If the |field_type| and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(types_seen, server_field_type, value)) {
      imported_profile.reset();
      local_imported_credit_card.reset();
      break;
    }

    types_seen.insert(server_field_type);

    if (group == CREDIT_CARD) {
      if (LowerCaseEqualsASCII(field->form_control_type, "month")) {
        DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, server_field_type);
        local_imported_credit_card->SetInfoForMonthInputType(value);
      } else {
        local_imported_credit_card->SetInfo(field_type, value, app_locale_);
      }
      ++importable_credit_card_fields;
    } else {
      // We need to store phone data in the variables, before building the whole
      // number at the end. The rest of the fields are set "as is".
      // If the fields are not the phone fields in question home.SetInfo() is
      // going to return false.
      if (!home.SetInfo(field_type, value))
        imported_profile->SetInfo(field_type, value, app_locale_);

      // Reject profiles with invalid country information.
      if (server_field_type == ADDRESS_HOME_COUNTRY &&
          !value.empty() &&
          imported_profile->GetRawInfo(ADDRESS_HOME_COUNTRY).empty()) {
        imported_profile.reset();
        break;
      }
    }
  }

  // Construct the phone number. Reject the profile if the number is invalid.
  if (imported_profile.get() && !home.IsEmpty()) {
    base::string16 constructed_number;
    if (!home.ParseNumber(*imported_profile, app_locale_,
                          &constructed_number) ||
        !imported_profile->SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                                   constructed_number,
                                   app_locale_)) {
      imported_profile.reset();
    }
  }

  // Reject the profile if minimum address and validation requirements are not
  // met.
  if (imported_profile.get() &&
      !IsValidLearnableProfile(*imported_profile, app_locale_))
    imported_profile.reset();

  // Reject the credit card if we did not detect enough filled credit card
  // fields or if the credit card number does not seem to be valid.
  if (local_imported_credit_card.get() &&
      !local_imported_credit_card->IsComplete()) {
    local_imported_credit_card.reset();
  }

  // Don't import if we already have this info.
  // Don't present an infobar if we have already saved this card number.
  bool merged_credit_card = false;
  if (local_imported_credit_card.get()) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards_.begin();
         iter != credit_cards_.end();
         ++iter) {
      // Make a local copy so that the data in |credit_cards_| isn't modified
      // directly by the UpdateFromImportedCard() call.
      CreditCard card = **iter;
      if (card.UpdateFromImportedCard(*local_imported_credit_card.get(),
                                      app_locale_)) {
        merged_credit_card = true;
        UpdateCreditCard(card);
        local_imported_credit_card.reset();
        break;
      }
    }
  }

  if (imported_profile.get()) {
    // We always save imported profiles.
    SaveImportedProfile(*imported_profile);
  }
  *imported_credit_card = local_imported_credit_card.Pass();

  if (imported_profile.get() || *imported_credit_card || merged_credit_card)
    return true;

  FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                    OnInsufficientFormData());
  return false;
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  if (is_off_the_record_)
    return;

  if (profile.IsEmpty(app_locale_))
    return;

  // Don't add an existing profile.
  if (FindByGUID<AutofillProfile>(web_profiles_, profile.guid()))
    return;

  if (!database_.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(web_profiles_, profile))
    return;

  // Add the new profile to the web database.
  database_->AddAutofillProfile(profile);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (is_off_the_record_)
    return;

  AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
  if (!existing_profile)
    return;

  // Don't overwrite the origin for a profile that is already stored.
  if (existing_profile->EqualsSansOrigin(profile))
    return;

  if (profile.IsEmpty(app_locale_)) {
    RemoveByGUID(profile.guid());
    return;
  }

  if (!database_.get())
    return;

  // Make the update.
  database_->UpdateAutofillProfile(profile);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

AutofillProfile* PersonalDataManager::GetProfileByGUID(
    const std::string& guid) {
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  std::vector<AutofillProfile*>::const_iterator iter =
      FindElementByGUID<AutofillProfile>(profiles, guid);
  return (iter != profiles.end()) ? *iter : NULL;
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  if (is_off_the_record_)
    return;

  if (credit_card.IsEmpty(app_locale_))
    return;

  if (FindByGUID<CreditCard>(credit_cards_, credit_card.guid()))
    return;

  if (!database_.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  if (is_off_the_record_)
    return;

  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (!existing_credit_card)
    return;

  // Don't overwrite the origin for a credit card that is already stored.
  if (existing_credit_card->Compare(credit_card) == 0)
    return;

  if (credit_card.IsEmpty(app_locale_)) {
    RemoveByGUID(credit_card.guid());
    return;
  }

  if (!database_.get())
    return;

  // Make the update.
  database_->UpdateCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (is_off_the_record_)
    return;

  bool is_credit_card = FindByGUID<CreditCard>(credit_cards_, guid);
  bool is_profile = !is_credit_card &&
      FindByGUID<AutofillProfile>(web_profiles_, guid);
  if (!is_credit_card && !is_profile)
    return;

  if (!database_.get())
    return;

  if (is_credit_card)
    database_->RemoveCreditCard(guid);
  else
    database_->RemoveAutofillProfile(guid);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

CreditCard* PersonalDataManager::GetCreditCardByGUID(const std::string& guid) {
  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  std::vector<CreditCard*>::const_iterator iter =
      FindElementByGUID<CreditCard>(credit_cards, guid);
  return (iter != credit_cards.end()) ? *iter : NULL;
}

void PersonalDataManager::GetNonEmptyTypes(
    ServerFieldTypeSet* non_empty_types) {
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    (*iter)->GetNonEmptyTypes(app_locale_, non_empty_types);
  }

  for (ScopedVector<CreditCard>::const_iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    (*iter)->GetNonEmptyTypes(app_locale_, non_empty_types);
  }
}

bool PersonalDataManager::IsDataLoaded() const {
  return is_data_loaded_;
}

const std::vector<AutofillProfile*>& PersonalDataManager::GetProfiles() const {
  return GetProfiles(false);
}

const std::vector<AutofillProfile*>& PersonalDataManager::web_profiles() const {
  return web_profiles_.get();
}

const std::vector<CreditCard*>& PersonalDataManager::GetCreditCards() const {
  return credit_cards_.get();
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
}

void PersonalDataManager::GetProfileSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    bool field_is_autofilled,
    const std::vector<ServerFieldType>& other_field_types,
    const base::Callback<bool(const AutofillProfile&)>& filter,
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<GUIDPair>* guid_pairs) {
  values->clear();
  labels->clear();
  icons->clear();
  guid_pairs->clear();

  const std::vector<AutofillProfile*>& profiles = GetProfiles(true);
  std::vector<AutofillProfile*> matched_profiles;
  for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    AutofillProfile* profile = *iter;

    // The value of the stored data for this field type in the |profile|.
    std::vector<base::string16> multi_values;
    AddressField address_field;
    if (i18n::FieldForType(type.GetStorableType(), &address_field) &&
        address_field == STREET_ADDRESS) {
      std::string street_address_line;
      GetStreetAddressLinesAsSingleLine(
          *i18n::CreateAddressDataFromAutofillProfile(*profile, app_locale_),
          &street_address_line);
      multi_values.push_back(base::UTF8ToUTF16(street_address_line));
    } else {
      profile->GetMultiInfo(type, app_locale_, &multi_values);
    }

    for (size_t i = 0; i < multi_values.size(); ++i) {
      // Newlines can be found only in a street address, which was collapsed
      // into a single line above.
      DCHECK(multi_values[i].find('\n') == std::string::npos);

      if (!field_is_autofilled) {
        // Suggest data that starts with what the user has typed.
        if (!multi_values[i].empty() &&
            StartsWith(multi_values[i], field_contents, false) &&
            (filter.is_null() || filter.Run(*profile))) {
          matched_profiles.push_back(profile);
          values->push_back(multi_values[i]);
          guid_pairs->push_back(GUIDPair(profile->guid(), i));
        }
      } else {
        if (multi_values[i].empty())
          continue;

        base::string16 profile_value_lower_case(
            base::StringToLowerASCII(multi_values[i]));
        base::string16 field_value_lower_case(
            base::StringToLowerASCII(field_contents));
        // Phone numbers could be split in US forms, so field value could be
        // either prefix or suffix of the phone.
        bool matched_phones = false;
        if (type.GetStorableType() == PHONE_HOME_NUMBER &&
            !field_value_lower_case.empty() &&
            profile_value_lower_case.find(field_value_lower_case) !=
                base::string16::npos) {
          matched_phones = true;
        }

        // Suggest variants of the profile that's already been filled in.
        if (matched_phones ||
            profile_value_lower_case == field_value_lower_case) {
          for (size_t j = 0; j < multi_values.size(); ++j) {
            if (!multi_values[j].empty()) {
              values->push_back(multi_values[j]);
              guid_pairs->push_back(GUIDPair(profile->guid(), j));
            }
          }

          // We've added all the values for this profile so move on to the
          // next.
          break;
        }
      }
    }
  }

  if (!field_is_autofilled) {
    AutofillProfile::CreateInferredLabels(
        matched_profiles, &other_field_types,
        type.GetStorableType(), 1, app_locale_, labels);
  } else {
    // No sub-labels for previously filled fields.
    labels->resize(values->size());
  }

  // No icons for profile suggestions.
  icons->resize(values->size());
}

void PersonalDataManager::GetCreditCardSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<GUIDPair>* guid_pairs) {
  values->clear();
  labels->clear();
  icons->clear();
  guid_pairs->clear();

  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    base::string16 creditcard_field_value =
        credit_card->GetInfo(type, app_locale_);
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field_contents, false)) {
      if (type.GetStorableType() == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      // If the value is the card number, the label is the expiration date.
      // Otherwise the label is the card number, or if that is empty the
      // cardholder name. The label should never repeat the value.
      base::string16 label;
      if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
        label = credit_card->GetInfo(
            AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale_);
      } else if (credit_card->number().empty()) {
        if (type.GetStorableType() != CREDIT_CARD_NAME) {
          label =
              credit_card->GetInfo(AutofillType(CREDIT_CARD_NAME), app_locale_);
        }
      } else {
        label = kCreditCardPrefix;
        label.append(credit_card->LastFourDigits());
      }

      values->push_back(creditcard_field_value);
      labels->push_back(label);
      icons->push_back(base::UTF8ToUTF16(credit_card->type()));
      guid_pairs->push_back(GUIDPair(credit_card->guid(), 0));
    }
  }
}

bool PersonalDataManager::IsAutofillEnabled() const {
  DCHECK(pref_service_);
  return pref_service_->GetBoolean(prefs::kAutofillEnabled);
}

std::string PersonalDataManager::CountryCodeForCurrentTimezone() const {
  return base::CountryCodeForCurrentTimezone();
}

void PersonalDataManager::SetPrefService(PrefService* pref_service) {
  enabled_pref_.reset(new BooleanPrefMember);
  pref_service_ = pref_service;
  // |pref_service_| can be NULL in tests.
  if (pref_service_) {
    enabled_pref_->Init(prefs::kAutofillEnabled, pref_service_,
        base::Bind(&PersonalDataManager::EnabledPrefChanged,
                   base::Unretained(this)));
  }
}

// static
bool PersonalDataManager::IsValidLearnableProfile(
    const AutofillProfile& profile,
    const std::string& app_locale) {
  if (!IsMinimumAddress(profile, app_locale))
    return false;

  base::string16 email = profile.GetRawInfo(EMAIL_ADDRESS);
  if (!email.empty() && !IsValidEmailAddress(email))
    return false;

  // Reject profiles with invalid US state information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_STATE))
    return false;

  // Reject profiles with invalid US zip information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_ZIP))
    return false;

  return true;
}

// static
std::string PersonalDataManager::MergeProfile(
    const AutofillProfile& new_profile,
    const std::vector<AutofillProfile*>& existing_profiles,
    const std::string& app_locale,
    std::vector<AutofillProfile>* merged_profiles) {
  merged_profiles->clear();

  // Set to true if |existing_profiles| already contains an equivalent profile.
  bool matching_profile_found = false;
  std::string guid = new_profile.guid();

  // If we have already saved this address, merge in any missing values.
  // Only merge with the first match.
  for (std::vector<AutofillProfile*>::const_iterator iter =
           existing_profiles.begin();
       iter != existing_profiles.end(); ++iter) {
    AutofillProfile* existing_profile = *iter;
    if (!matching_profile_found &&
        !new_profile.PrimaryValue().empty() &&
        base::StringToLowerASCII(existing_profile->PrimaryValue()) ==
            base::StringToLowerASCII(new_profile.PrimaryValue())) {
      // Unverified profiles should always be updated with the newer data,
      // whereas verified profiles should only ever be overwritten by verified
      // data.  If an automatically aggregated profile would overwrite a
      // verified profile, just drop it.
      matching_profile_found = true;
      guid = existing_profile->guid();
      if (!existing_profile->IsVerified() || new_profile.IsVerified())
        existing_profile->OverwriteWithOrAddTo(new_profile, app_locale);
    }
    merged_profiles->push_back(*existing_profile);
  }

  // If the new profile was not merged with an existing one, add it to the list.
  if (!matching_profile_found)
    merged_profiles->push_back(new_profile);

  return guid;
}

bool PersonalDataManager::IsCountryOfInterest(const std::string& country_code)
    const {
  DCHECK_EQ(2U, country_code.size());

  const std::vector<AutofillProfile*>& profiles = web_profiles();
  std::list<std::string> country_codes;
  for (size_t i = 0; i < profiles.size(); ++i) {
    country_codes.push_back(base::StringToLowerASCII(base::UTF16ToASCII(
        profiles[i]->GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }

  std::string timezone_country = CountryCodeForCurrentTimezone();
  if (!timezone_country.empty())
    country_codes.push_back(base::StringToLowerASCII(timezone_country));

  // Only take the locale into consideration if all else fails.
  if (country_codes.empty()) {
    country_codes.push_back(base::StringToLowerASCII(
        AutofillCountry::CountryCodeForLocale(app_locale())));
  }

  return std::find(country_codes.begin(), country_codes.end(),
                   base::StringToLowerASCII(country_code)) !=
                       country_codes.end();
}

const std::string& PersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    default_country_code_ = MostCommonCountryCodeFromProfiles();

  // Failing that, guess based on system timezone.
  if (default_country_code_.empty())
    default_country_code_ = CountryCodeForCurrentTimezone();

  // Failing that, guess based on locale.
  if (default_country_code_.empty())
    default_country_code_ = AutofillCountry::CountryCodeForLocale(app_locale());

  return default_country_code_;
}

void PersonalDataManager::SetProfiles(std::vector<AutofillProfile>* profiles) {
  if (is_off_the_record_)
    return;

  // Remove empty profiles from input.
  profiles->erase(std::remove_if(profiles->begin(), profiles->end(),
                                 IsEmptyFunctor<AutofillProfile>(app_locale_)),
                  profiles->end());

  if (!database_.get())
    return;

  // Any profiles that are not in the new profile list should be removed from
  // the web database.
  for (std::vector<AutofillProfile*>::const_iterator iter =
           web_profiles_.begin();
       iter != web_profiles_.end(); ++iter) {
    if (!FindByGUID<AutofillProfile>(*profiles, (*iter)->guid()))
      database_->RemoveAutofillProfile((*iter)->guid());
  }

  // Update the web database with the existing profiles.
  for (std::vector<AutofillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (FindByGUID<AutofillProfile>(web_profiles_, iter->guid()))
      database_->UpdateAutofillProfile(*iter);
  }

  // Add the new profiles to the web database.  Don't add a duplicate.
  for (std::vector<AutofillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (!FindByGUID<AutofillProfile>(web_profiles_, iter->guid()) &&
        !FindByContents(web_profiles_, *iter))
      database_->AddAutofillProfile(*iter);
  }

  // Copy in the new profiles.
  web_profiles_.clear();
  for (std::vector<AutofillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    web_profiles_.push_back(new AutofillProfile(*iter));
  }

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  if (is_off_the_record_)
    return;

  // Remove empty credit cards from input.
  credit_cards->erase(std::remove_if(credit_cards->begin(), credit_cards->end(),
                                     IsEmptyFunctor<CreditCard>(app_locale_)),
                      credit_cards->end());

  if (!database_.get())
    return;

  // Any credit cards that are not in the new credit card list should be
  // removed.
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    if (!FindByGUID<CreditCard>(*credit_cards, (*iter)->guid()))
      database_->RemoveCreditCard((*iter)->guid());
  }

  // Update the web database with the existing credit cards.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (FindByGUID<CreditCard>(credit_cards_, iter->guid()))
      database_->UpdateCreditCard(*iter);
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (!FindByGUID<CreditCard>(credit_cards_, iter->guid()) &&
        !FindByContents(credit_cards_, *iter))
      database_->AddCreditCard(*iter);
  }

  // Copy in the new credit cards.
  credit_cards_.clear();
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    credit_cards_.push_back(new CreditCard(*iter));
  }

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::LoadProfiles() {
  if (!database_.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_profiles_query_);

  pending_profiles_query_ = database_->GetAutofillProfiles(this);
}

// Win, Linux, and iOS implementations do nothing. Mac and Android
// implementations fill in the contents of |auxiliary_profiles_|.
#if defined(OS_IOS) || (!defined(OS_MACOSX) && !defined(OS_ANDROID))
void PersonalDataManager::LoadAuxiliaryProfiles(bool record_metrics) const {
}
#endif

void PersonalDataManager::LoadCreditCards() {
  if (!database_.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_creditcards_query_);

  pending_creditcards_query_ = database_->GetCreditCards(this);
}

void PersonalDataManager::ReceiveLoadedProfiles(WebDataServiceBase::Handle h,
                                                const WDTypedResult* result) {
  DCHECK_EQ(pending_profiles_query_, h);

  pending_profiles_query_ = 0;
  web_profiles_.clear();

  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);

  std::vector<AutofillProfile*> profiles = r->GetValue();
  for (std::vector<AutofillProfile*>::iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    web_profiles_.push_back(*iter);
  }

  LogProfileCount();
}

void PersonalDataManager::ReceiveLoadedCreditCards(
    WebDataServiceBase::Handle h, const WDTypedResult* result) {
  DCHECK_EQ(pending_creditcards_query_, h);

  pending_creditcards_query_ = 0;
  credit_cards_.clear();

  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  for (std::vector<CreditCard*>::iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    credit_cards_.push_back(*iter);
  }
}

void PersonalDataManager::CancelPendingQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_.get()) {
      NOTREACHED();
      return;
    }
    database_->CancelRequest(*handle);
  }
  *handle = 0;
}

std::string PersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  if (is_off_the_record_)
    return std::string();

  // Don't save a web profile if the data in the profile is a subset of an
  // auxiliary profile.
  for (std::vector<AutofillProfile*>::const_iterator iter =
           auxiliary_profiles_.begin();
       iter != auxiliary_profiles_.end(); ++iter) {
    if (imported_profile.IsSubsetOf(**iter, app_locale_))
      return (*iter)->guid();
  }

  std::vector<AutofillProfile> profiles;
  std::string guid =
      MergeProfile(imported_profile, web_profiles_.get(), app_locale_,
                   &profiles);
  SetProfiles(&profiles);
  return guid;
}

void PersonalDataManager::NotifyPersonalDataChanged() {
  FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                    OnPersonalDataChanged());
}

std::string PersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  if (is_off_the_record_)
    return std::string();

  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;

  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards_.begin();
       iter != credit_cards_.end();
       ++iter) {
    CreditCard* card = *iter;
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged && card->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = card->guid();
      merged = true;
    }

    credit_cards.push_back(*card);
  }

  if (!merged)
    credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);
  return guid;
}

void PersonalDataManager::LogProfileCount() const {
  if (!has_logged_profile_count_) {
    metric_logger_->LogStoredProfileCount(web_profiles_.size());
    has_logged_profile_count_ = true;
  }
}

std::string PersonalDataManager::MostCommonCountryCodeFromProfiles() const {
  if (!IsAutofillEnabled())
    return std::string();

  // Count up country codes from existing profiles.
  std::map<std::string, int> votes;
  // TODO(estade): can we make this GetProfiles() instead? It seems to cause
  // errors in tests on mac trybots. See http://crbug.com/57221
  const std::vector<AutofillProfile*>& profiles = web_profiles();
  std::vector<std::string> country_codes;
  AutofillCountry::GetAvailableCountries(&country_codes);
  for (size_t i = 0; i < profiles.size(); ++i) {
    std::string country_code = StringToUpperASCII(base::UTF16ToASCII(
        profiles[i]->GetRawInfo(ADDRESS_HOME_COUNTRY)));

    if (std::find(country_codes.begin(), country_codes.end(), country_code) !=
            country_codes.end()) {
      // Verified profiles count 100x more than unverified ones.
      votes[country_code] += profiles[i]->IsVerified() ? 100 : 1;
    }
  }

  // Take the most common country code.
  if (!votes.empty()) {
    std::map<std::string, int>::iterator iter =
        std::max_element(votes.begin(), votes.end(), CompareVotes);
    return iter->first;
  }

  return std::string();
}

void PersonalDataManager::EnabledPrefChanged() {
  default_country_code_.clear();
  NotifyPersonalDataChanged();
}

const std::vector<AutofillProfile*>& PersonalDataManager::GetProfiles(
    bool record_metrics) const {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (!pref_service_->GetBoolean(prefs::kAutofillUseMacAddressBook))
    return web_profiles();
#else
  if (!pref_service_->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled))
    return web_profiles();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  profiles_.clear();

  // Populates |auxiliary_profiles_|.
  LoadAuxiliaryProfiles(record_metrics);

  profiles_.insert(profiles_.end(), web_profiles_.begin(), web_profiles_.end());
  profiles_.insert(
      profiles_.end(), auxiliary_profiles_.begin(), auxiliary_profiles_.end());
  return profiles_;
}

}  // namespace autofill
