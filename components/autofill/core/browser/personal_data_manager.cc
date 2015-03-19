// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include "base/command_line.h"
#include "base/i18n/timezone.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill-inl.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace autofill {
namespace {

using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

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

// Returns the list of values for the given type in the profile. There may be
// more than one (for example, the user can have more than one phone number per
// address).
//
// In addition to just getting the values out of the autocomplete profile, this
// function handles formatting of the street address into a single string.
std::vector<base::string16> GetMultiInfoInOneLine(
    const AutofillProfile* profile,
    const AutofillType& type,
    const std::string app_locale) {
  std::vector<base::string16> results;

  AddressField address_field;
  if (i18n::FieldForType(type.GetStorableType(), &address_field) &&
      address_field == STREET_ADDRESS) {
    std::string street_address_line;
    GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(*profile, app_locale),
        &street_address_line);
    results.push_back(base::UTF8ToUTF16(street_address_line));
  } else {
    profile->GetMultiInfo(type, app_locale, &results);
  }
  return results;
}

// Returns true if the current field contents match what's currently in the
// form field. The current field contents must be already canonicalized. In
// addition to doing a case-insensitive match, this will do special handling
// for phone numbers.
bool MatchesInput(const base::string16& profile_value,
                  const base::string16& field_contents_canon,
                  const AutofillType& type) {
  base::string16 profile_value_canon =
      AutofillProfile::CanonicalizeProfileString(profile_value);

  if (profile_value_canon == field_contents_canon)
    return true;

  // Phone numbers could be split in US forms, so field value could be
  // either prefix or suffix of the phone.
  if (type.GetStorableType() == PHONE_HOME_NUMBER) {
    return !field_contents_canon.empty() &&
           profile_value_canon.find(field_contents_canon) !=
               base::string16::npos;
  }

  return false;
}

// Receives the loaded profiles from the web data service and stores them in
// |*dest|. The pending handle is the address of the pending handle
// corresponding to this request type. This function is used to save both
// server and local profiles and credit cards.
template <typename ValueType>
void ReceiveLoadedDbValues(WebDataServiceBase::Handle h,
                           const WDTypedResult* result,
                           WebDataServiceBase::Handle* pending_handle,
                           ScopedVector<ValueType>* dest) {
  DCHECK_EQ(*pending_handle, h);
  *pending_handle = 0;

  const WDResult<std::vector<ValueType*>>* r =
      static_cast<const WDResult<std::vector<ValueType*>>*>(result);

  dest->clear();
  for (ValueType* value : r->GetValue())
    dest->push_back(value);
}

// A helper function for finding the maximum value in a string->int map.
static bool CompareVotes(const std::pair<std::string, int>& a,
                         const std::pair<std::string, int>& b) {
  return a.second < b.second;
}

// Ranks two data models according to their recency of use. Currently this will
// place all server (Wallet) cards and addresses below all locally saved ones,
// which is probably not what we want. TODO(estade): figure out relative ranking
// of server data.
bool RankByMfu(const AutofillDataModel* a, const AutofillDataModel* b) {
  if (a->use_count() != b->use_count())
    return a->use_count() > b->use_count();

  // Ties are broken by MRU.
  return a->use_date() > b->use_date();
}

}  // namespace

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : database_(NULL),
      is_data_loaded_(false),
      pending_profiles_query_(0),
      pending_server_profiles_query_(0),
      pending_creditcards_query_(0),
      pending_server_creditcards_query_(0),
      app_locale_(app_locale),
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
    AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  // WebDataService may not be available in tests.
  if (!database_.get())
    return;

  LoadProfiles();
  LoadCreditCards();

  database_->AddObserver(this);
}

PersonalDataManager::~PersonalDataManager() {
  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_server_profiles_query_);
  CancelPendingQuery(&pending_creditcards_query_);
  CancelPendingQuery(&pending_server_creditcards_query_);

  if (database_.get())
    database_->RemoveObserver(this);
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_profiles_query_ || pending_server_profiles_query_ ||
         pending_creditcards_query_ || pending_server_creditcards_query_);

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 PersonalDataManager::OnWebDataServiceRequestDone"));

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
      if (h == pending_profiles_query_) {
        ReceiveLoadedDbValues(h, result, &pending_profiles_query_,
                              &web_profiles_);
        LogProfileCount();  // This only logs local profiles.
      } else {
        ReceiveLoadedDbValues(h, result, &pending_server_profiles_query_,
                              &server_profiles_);

        if (!server_profiles_.empty()) {
          base::string16 email = base::UTF8ToUTF16(
              pref_service_->GetString(::prefs::kGoogleServicesUsername));
          DCHECK(!email.empty());
          for (AutofillProfile* profile : server_profiles_)
            profile->SetRawInfo(EMAIL_ADDRESS, email);
        }
      }
      break;
    case AUTOFILL_CREDITCARDS_RESULT:
      if (h == pending_creditcards_query_) {
        ReceiveLoadedDbValues(h, result, &pending_creditcards_query_,
                              &local_credit_cards_);
      } else {
        ReceiveLoadedDbValues(h, result, &pending_server_creditcards_query_,
                              &server_credit_cards_);
      }
      break;
    default:
      NOTREACHED();
  }

  // If all requests have responded, then all personal data is loaded.
  if (pending_profiles_query_ == 0 &&
      pending_creditcards_query_ == 0 &&
      pending_server_profiles_query_ == 0 &&
      pending_server_creditcards_query_ == 0) {
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
  if (local_imported_credit_card) {
    for (CreditCard* card : local_credit_cards_) {
      // Make a local copy so that the data in |local_credit_cards_| isn't
      // modified directly by the UpdateFromImportedCard() call.
      CreditCard card_copy(*card);
      if (card_copy.UpdateFromImportedCard(*local_imported_credit_card.get(),
                                           app_locale_)) {
        merged_credit_card = true;
        UpdateCreditCard(card_copy);
        local_imported_credit_card.reset();
        break;
      }
    }
  }

  // Also don't offer to save if we already have this stored as a full wallet
  // card. (In particular this comes up just after filling and submitting a
  // Wallet card.)
  if (local_imported_credit_card) {
    for (CreditCard* card : server_credit_cards_) {
      if (card->record_type() == CreditCard::FULL_SERVER_CARD &&
          local_imported_credit_card->IsLocalDuplicateOfServerCard(*card)) {
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

void PersonalDataManager::RecordUseOf(const AutofillDataModel& data_model) {
  if (!database_.get())
    return;

  CreditCard* credit_card = GetCreditCardByGUID(data_model.guid());
  if (credit_card) {
    credit_card->RecordUse();

    if (credit_card->record_type() == CreditCard::LOCAL_CARD) {
      database_->UpdateCreditCard(*credit_card);
    } else if (credit_card->record_type() == CreditCard::FULL_SERVER_CARD) {
      database_->UpdateUnmaskedCardUsageStats(*credit_card);
    } else {
      // It's possible to get a masked server card here if the user decides not
      // to store a card while verifying it. We don't currently track usage
      // of masked cards, so no-op.
      return;
    }

    Refresh();
    return;
  }

  AutofillProfile* profile = GetProfileByGUID(data_model.guid());
  if (profile) {
    profile->RecordUse();
    database_->UpdateAutofillProfile(*profile);
    Refresh();
  }
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

  if (FindByGUID<CreditCard>(local_credit_cards_, credit_card.guid()))
    return;

  if (!database_.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(local_credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::LOCAL_CARD, credit_card.record_type());
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

void PersonalDataManager::UpdateServerCreditCard(
    const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  if (is_off_the_record_ || !database_.get())
    return;

  // Look up by server id, not GUID.
  CreditCard* existing_credit_card = nullptr;
  for (auto it : server_credit_cards_) {
    if (credit_card.server_id() == it->server_id()) {
      existing_credit_card = it;
      break;
    }
  }
  if (!existing_credit_card)
    return;

  DCHECK_NE(existing_credit_card->record_type(), credit_card.record_type());
  DCHECK_EQ(existing_credit_card->Label(), credit_card.Label());
  if (existing_credit_card->record_type() == CreditCard::MASKED_SERVER_CARD) {
    database_->UnmaskServerCreditCard(credit_card.server_id(),
                                      credit_card.number());
  } else {
    database_->MaskServerCreditCard(credit_card.server_id());
  }

  Refresh();
}

void PersonalDataManager::ResetFullServerCard(const std::string& guid) {
  for (const CreditCard* card : server_credit_cards_) {
    if (card->guid() == guid) {
      DCHECK_EQ(card->record_type(), CreditCard::FULL_SERVER_CARD);
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::MASKED_SERVER_CARD);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
      break;
    }
  }
}

void PersonalDataManager::ResetFullServerCards() {
  for (const CreditCard* card : server_credit_cards_) {
    CreditCard card_copy = *card;
    if (card_copy.record_type() == CreditCard::FULL_SERVER_CARD) {
      card_copy.set_record_type(CreditCard::MASKED_SERVER_CARD);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
    }
  }
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (is_off_the_record_)
    return;

  bool is_credit_card = FindByGUID<CreditCard>(local_credit_cards_, guid);
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
  for (AutofillProfile* profile : GetProfiles())
    profile->GetNonEmptyTypes(app_locale_, non_empty_types);
  for (CreditCard* card : GetCreditCards())
    card->GetNonEmptyTypes(app_locale_, non_empty_types);
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

const std::vector<CreditCard*>& PersonalDataManager::GetLocalCreditCards()
    const {
  return local_credit_cards_.get();
}

const std::vector<CreditCard*>& PersonalDataManager::GetCreditCards() const {
  credit_cards_.clear();
  credit_cards_.insert(credit_cards_.end(), local_credit_cards_.begin(),
                       local_credit_cards_.end());
  if (IsExperimentalWalletIntegrationEnabled() &&
      pref_service_->GetBoolean(prefs::kAutofillWalletImportEnabled)) {
    credit_cards_.insert(credit_cards_.end(), server_credit_cards_.begin(),
                         server_credit_cards_.end());
  }
  return credit_cards_;
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
}

std::vector<Suggestion> PersonalDataManager::GetProfileSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    bool field_is_autofilled,
    const std::vector<ServerFieldType>& other_field_types) {
  base::string16 field_contents_canon =
      AutofillProfile::CanonicalizeProfileString(field_contents);

  std::vector<AutofillProfile*> profiles = GetProfiles(true);
  std::sort(profiles.begin(), profiles.end(), RankByMfu);

  if (field_is_autofilled) {
    std::vector<Suggestion> suggestions;
    // This field was previously autofilled. In this case, suggesting results
    // based on prefix is useless since it will be the same thing. Instead,
    // check for a field that may have multiple possible values (for example,
    // multiple names for the same address) and suggest the alternates. This
    // allows for easy correction of the data.
    for (AutofillProfile* profile : profiles) {
      std::vector<base::string16> values =
          GetMultiInfoInOneLine(profile, type, app_locale_);

      // Check if the contents of this field match any of the inputs.
      bool matches_field = false;
      for (const base::string16& value : values) {
        if (MatchesInput(value, field_contents_canon, type)) {
          matches_field = true;
          break;
        }
      }

      if (matches_field) {
        // Field unmodified, make alternate suggestions.
        for (size_t i = 0; i < values.size(); i++) {
          if (values[i].empty())
            continue;

          bool is_unique = true;
          for (size_t j = 0; j < suggestions.size(); ++j) {
            if (values[i] == suggestions[j].value) {
              is_unique = false;
              break;
            }
          }
          if (is_unique) {
            suggestions.push_back(Suggestion(values[i]));
            suggestions.back().backend_id.guid = profile->guid();
            suggestions.back().backend_id.variant = i;
          }
        }
      }
    }

    return suggestions;
  }

  std::vector<Suggestion> suggestions;
  // Match based on a prefix search.
  std::vector<AutofillProfile*> matched_profiles;
  for (AutofillProfile* profile : profiles) {
    std::vector<base::string16> values =
        GetMultiInfoInOneLine(profile, type, app_locale_);
    for (size_t i = 0; i < values.size(); i++) {
      if (values[i].empty())
        continue;

      base::string16 value_canon =
          AutofillProfile::CanonicalizeProfileString(values[i]);
      if (StartsWith(value_canon, field_contents_canon, true)) {
        // Prefix match, add suggestion.
        matched_profiles.push_back(profile);
        suggestions.push_back(Suggestion(values[i]));
        suggestions.back().backend_id.guid = profile->guid();
        suggestions.back().backend_id.variant = i;
      }
    }
  }

  // Don't show two suggestions if one is a subset of the other.
  std::vector<AutofillProfile*> unique_matched_profiles;
  std::vector<Suggestion> unique_suggestions;
  ServerFieldTypeSet types(other_field_types.begin(), other_field_types.end());
  for (size_t i = 0; i < matched_profiles.size(); ++i) {
    bool include = true;
    AutofillProfile* profile_a = matched_profiles[i];
    for (size_t j = 0; j < matched_profiles.size(); ++j) {
      AutofillProfile* profile_b = matched_profiles[j];
      // Check if profile A is a subset of profile B. If not, continue.
      if (i == j || suggestions[i].value != suggestions[j].value ||
          !profile_a->IsSubsetOfForFieldSet(*profile_b, app_locale_, types)) {
        continue;
      }

      // Check if profile B is also a subset of profile A. If so, the
      // profiles are identical. Include the first one but not the second.
      if (i < j &&
          profile_b->IsSubsetOfForFieldSet(*profile_a, app_locale_, types)) {
        continue;
      }

      // One-way subset. Don't include profile A.
      include = false;
      break;
    }
    if (include) {
      unique_matched_profiles.push_back(matched_profiles[i]);
      unique_suggestions.push_back(suggestions[i]);
    }
  }

  // Generate disambiguating labels based on the list of matches.
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(
      unique_matched_profiles, &other_field_types, type.GetStorableType(), 1,
      app_locale_, &labels);
  DCHECK_EQ(unique_suggestions.size(), labels.size());
  for (size_t i = 0; i < labels.size(); i++)
    unique_suggestions[i].label = labels[i];

  return unique_suggestions;
}

std::vector<Suggestion> PersonalDataManager::GetCreditCardSuggestions(
    const AutofillType& type,
    const base::string16& field_contents) {
  std::list<const CreditCard*> cards_to_suggest;
  for (const CreditCard* credit_card : GetCreditCards()) {
    // The value of the stored data for this field type in the |credit_card|.
    base::string16 creditcard_field_value =
        credit_card->GetInfo(type, app_locale_);
    if (creditcard_field_value.empty())
      continue;

    // For card number fields, suggest the card if:
    // - the number matches any part of the card, or
    // - it's a masked card and there are 6 or fewers typed so far.
    // For other fields, require that the field contents match the beginning of
    // the stored data.
    if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
      if (creditcard_field_value.find(field_contents) == base::string16::npos &&
          (credit_card->record_type() != CreditCard::MASKED_SERVER_CARD ||
           field_contents.size() >= 6)) {
        continue;
      }
    } else if (!StartsWith(creditcard_field_value, field_contents, false)) {
      continue;
    }

    cards_to_suggest.push_back(credit_card);
  }

  // Server cards shadow identical local cards.
  for (auto outer_it = cards_to_suggest.begin();
       outer_it != cards_to_suggest.end();
       ++outer_it) {
    if ((*outer_it)->record_type() == CreditCard::LOCAL_CARD)
      continue;

    for (auto inner_it = cards_to_suggest.begin();
         inner_it != cards_to_suggest.end();) {
      auto inner_it_copy = inner_it++;
      if ((*inner_it_copy)->IsLocalDuplicateOfServerCard(**outer_it))
        cards_to_suggest.erase(inner_it_copy);
    }
  }

  cards_to_suggest.sort(RankByMfu);

  std::vector<Suggestion> suggestions;
  for (const CreditCard* credit_card : cards_to_suggest) {
    // Make a new suggestion.
    suggestions.push_back(Suggestion());
    Suggestion* suggestion = &suggestions.back();

    suggestion->value = credit_card->GetInfo(type, app_locale_);
    suggestion->icon = base::UTF8ToUTF16(credit_card->type());
    suggestion->backend_id.guid = credit_card->guid();

    // If the value is the card number, the label is the expiration date.
    // Otherwise the label is the card number, or if that is empty the
    // cardholder name. The label should never repeat the value.
    if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
      suggestion->value = credit_card->TypeAndLastFourDigits();
      suggestion->label = credit_card->GetInfo(
          AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale_);
    } else if (credit_card->number().empty()) {
      if (type.GetStorableType() != CREDIT_CARD_NAME) {
        suggestion->label =
            credit_card->GetInfo(AutofillType(CREDIT_CARD_NAME), app_locale_);
      }
    } else {
#if defined(OS_ANDROID)
      // Since Android places the label on its own row, there's more horizontal
      // space to work with. Show "Amex - 1234" rather than desktop's "*1234".
      suggestion->label = credit_card->TypeAndLastFourDigits();
#else
      suggestion->label = base::ASCIIToUTF16("*");
      suggestion->label.append(credit_card->LastFourDigits());
#endif
    }
  }
  return suggestions;
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
  wallet_enabled_pref_.reset(new BooleanPrefMember);
  pref_service_ = pref_service;
  // |pref_service_| can be NULL in tests.
  if (pref_service_) {
    enabled_pref_->Init(prefs::kAutofillEnabled, pref_service_,
        base::Bind(&PersonalDataManager::EnabledPrefChanged,
                   base::Unretained(this)));
    wallet_enabled_pref_->Init(prefs::kAutofillWalletImportEnabled,
        pref_service_,
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
  for (AutofillProfile* existing_profile : existing_profiles) {
    if (!matching_profile_found &&
        !new_profile.PrimaryValue().empty() &&
        AutofillProfile::AreProfileStringsSimilar(
            existing_profile->PrimaryValue(),
            new_profile.PrimaryValue())) {
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

bool PersonalDataManager::IsExperimentalWalletIntegrationEnabled() const {
  return pref_service_->GetBoolean(prefs::kAutofillWalletSyncExperimentEnabled);
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
  for (const CreditCard* card : local_credit_cards_) {
    if (!FindByGUID<CreditCard>(*credit_cards, card->guid()))
      database_->RemoveCreditCard(card->guid());
  }

  // Update the web database with the existing credit cards.
  for (const CreditCard& card : *credit_cards) {
    if (FindByGUID<CreditCard>(local_credit_cards_, card.guid()))
      database_->UpdateCreditCard(card);
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (const CreditCard& card : *credit_cards) {
    if (!FindByGUID<CreditCard>(local_credit_cards_, card.guid()) &&
        !FindByContents(local_credit_cards_, card))
      database_->AddCreditCard(card);
  }

  // Copy in the new credit cards.
  local_credit_cards_.clear();
  for (const CreditCard& card : *credit_cards)
    local_credit_cards_.push_back(new CreditCard(card));

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::LoadProfiles() {
  if (!database_.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_server_profiles_query_);

  pending_profiles_query_ = database_->GetAutofillProfiles(this);
  pending_server_profiles_query_ = database_->GetServerProfiles(this);
}

// Win, Linux, Android and iOS implementations do nothing. Mac implementation
// fills in the contents of |auxiliary_profiles_|.
#if defined(OS_IOS) || !defined(OS_MACOSX)
void PersonalDataManager::LoadAuxiliaryProfiles(bool record_metrics) const {
}
#endif

void PersonalDataManager::LoadCreditCards() {
  if (!database_.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_creditcards_query_);
  CancelPendingQuery(&pending_server_creditcards_query_);

  pending_creditcards_query_ = database_->GetCreditCards(this);
  pending_server_creditcards_query_ = database_->GetServerCreditCards(this);
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
  // auxiliary profile...
  for (AutofillProfile* profile : auxiliary_profiles_) {
    if (imported_profile.IsSubsetOf(*profile, app_locale_))
      return profile->guid();
  }

  // ...or server profile.
  for (AutofillProfile* profile : server_profiles_) {
    if (imported_profile.IsSubsetOf(*profile, app_locale_))
      return profile->guid();
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
  for (CreditCard* card : local_credit_cards_) {
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
    AutofillMetrics::LogStoredProfileCount(web_profiles_.size());
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
  if (!pref_service_->GetBoolean(prefs::kAutofillWalletImportEnabled)) {
    // Re-mask all server cards when the user turns off wallet card
    // integration.
    ResetFullServerCards();
  }
  NotifyPersonalDataChanged();
}

const std::vector<AutofillProfile*>& PersonalDataManager::GetProfiles(
    bool record_metrics) const {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  bool use_auxiliary_profiles =
      pref_service_->GetBoolean(prefs::kAutofillUseMacAddressBook);
#else
  bool use_auxiliary_profiles =
      pref_service_->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  profiles_.clear();
  profiles_.insert(profiles_.end(), web_profiles().begin(),
                   web_profiles().end());
  if (use_auxiliary_profiles) {
    LoadAuxiliaryProfiles(record_metrics);
    profiles_.insert(
        profiles_.end(), auxiliary_profiles_.begin(),
        auxiliary_profiles_.end());
  }
  if (IsExperimentalWalletIntegrationEnabled() &&
      pref_service_->GetBoolean(prefs::kAutofillWalletImportEnabled)) {
    profiles_.insert(
        profiles_.end(), server_profiles_.begin(), server_profiles_.end());
  }
  return profiles_;
}

}  // namespace autofill
