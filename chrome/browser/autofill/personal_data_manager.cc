// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace {

// The minimum number of fields that must contain user data and have known types
// before AutoFill will attempt to import the data into a profile.
const int kMinImportSize = 3;

const char kUnlabeled[] = "Unlabeled";

template<typename T>
class FormGroupIDMatchesFunctor {
 public:
  explicit FormGroupIDMatchesFunctor(int id) : id_(id) {}

  bool operator()(const T& form_group) {
    return form_group.unique_id() == id_;
  }

 private:
  int id_;
};

template<typename T>
class DereferenceFunctor {
 public:
  template<typename T_Iterator>
  const T& operator()(const T_Iterator& iterator) {
    return *iterator;
  }
};

}  // namespace

PersonalDataManager::~PersonalDataManager() {
  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_creditcards_query_);
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  // Error from the web database.
  if (!result)
    return;

  DCHECK(pending_profiles_query_ || pending_creditcards_query_);
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT ||
         result->GetType() == AUTOFILL_CREDITCARDS_RESULT);

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
    FOR_EACH_OBSERVER(Observer, observers_, OnPersonalDataLoaded());
  }
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager,
// views::ButtonListener implementations
void PersonalDataManager::OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
  // |profiles| may be NULL.
  // |credit_cards| may be NULL.
  if (profiles) {
    CancelPendingQuery(&pending_profiles_query_);
    SetProfiles(profiles);
  }
  if (credit_cards) {
    CancelPendingQuery(&pending_creditcards_query_);
    SetCreditCards(credit_cards);
  }
}

void PersonalDataManager::SetObserver(PersonalDataManager::Observer* observer) {
  // TODO: RemoveObserver is for compatibility with old code, it should be
  // nuked.
  observers_.RemoveObserver(observer);
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PersonalDataManager::ImportFormData(
    const std::vector<FormStructure*>& form_structures,
    AutoFillManager* autofill_manager) {
  AutoLock lock(unique_ids_lock_);
  // Parse the form and construct a profile based on the information that is
  // possible to import.
  int importable_fields = 0;
  int importable_credit_card_fields = 0;
  imported_profile_.reset(new AutoFillProfile(string16(), 0));
  // TODO(jhawkins): Use a hash of the CC# instead of a list of unique IDs?
  imported_credit_card_.reset(new CreditCard(string16(), 0));

  bool billing_address_info = false;
  std::vector<FormStructure*>::const_iterator iter;
  for (iter = form_structures.begin(); iter != form_structures.end(); ++iter) {
    const FormStructure* form = *iter;
    for (size_t i = 0; i < form->field_count(); ++i) {
      const AutoFillField* field = form->field(i);
      string16 value = CollapseWhitespace(field->value(), false);

      // If we don't know the type of the field, or the user hasn't entered any
      // information into the field, then skip it.
      if (!field->IsFieldFillable() || value.empty())
        continue;

      AutoFillType field_type(field->type());
      FieldTypeGroup group(field_type.group());

      if (group == AutoFillType::CREDIT_CARD) {
        // If the user has a password set, we have no way of setting credit
        // card numbers.
        if (!HasPassword()) {
          imported_credit_card_->SetInfo(AutoFillType(field_type.field_type()),
                                         value);
          ++importable_credit_card_fields;
        }
      } else {
        // In the case of a phone number, if the whole phone number was entered
        // into a single field, then parse it and set the sub components.
        if (field_type.subgroup() == AutoFillType::PHONE_WHOLE_NUMBER) {
          string16 number;
          string16 city_code;
          string16 country_code;
          if (group == AutoFillType::PHONE_HOME) {
            PhoneNumber::ParsePhoneNumber(
                value, &number, &city_code, &country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_HOME_COUNTRY_CODE), country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_HOME_CITY_CODE), city_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_HOME_NUMBER), number);
          } else if (group == AutoFillType::PHONE_FAX) {
            PhoneNumber::ParsePhoneNumber(
                value, &number, &city_code, &country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_FAX_COUNTRY_CODE), country_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_FAX_CITY_CODE), city_code);
            imported_profile_->SetInfo(
                AutoFillType(PHONE_FAX_NUMBER), number);
          }
          continue;
        }

        imported_profile_->SetInfo(AutoFillType(field_type.field_type()),
                                   value);

        // If we found any billing address information, then set the profile to
        // use a separate billing address.
        if (group == AutoFillType::ADDRESS_BILLING)
          billing_address_info = true;

        ++importable_fields;
      }
    }
  }

  // If the user did not enter enough information on the page then don't bother
  // importing the data.
  if (importable_fields + importable_credit_card_fields < kMinImportSize)
    return false;

  if (importable_fields == 0)
    imported_profile_.reset();

  if (importable_credit_card_fields == 0)
    imported_credit_card_.reset();

  {
    // We're now done with the unique IDs, and SaveImportedProfile() needs the
    // lock, so release it.
    AutoUnlock unlock(unique_ids_lock_);

    // We always save imported profiles.
    SaveImportedProfile();

    // We never save an imported credit card at this point. If there was one we
    // found, we'll be asked to save it later once the user gives their OK.
  }

  return true;
}

void PersonalDataManager::GetImportedFormData(AutoFillProfile** profile,
                                              CreditCard** credit_card) {
  DCHECK(profile);
  DCHECK(credit_card);

  if (imported_profile_.get()) {
    imported_profile_->set_label(ASCIIToUTF16(kUnlabeled));
  }
  *profile = imported_profile_.get();

  if (imported_credit_card_.get()) {
    imported_credit_card_->set_label(ASCIIToUTF16(kUnlabeled));
  }
  *credit_card = imported_credit_card_.get();
}

void PersonalDataManager::SetProfiles(std::vector<AutoFillProfile>* profiles) {
  if (profile_->IsOffTheRecord())
    return;

  // Remove empty profiles from input.
  profiles->erase(
      std::remove_if(profiles->begin(), profiles->end(),
                     std::mem_fun_ref(&AutoFillProfile::IsEmpty)),
      profiles->end());

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!wds)
    return;

  AutoLock lock(unique_ids_lock_);

  // Remove the unique IDs of the new set of profiles from the unique ID set.
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (iter->unique_id() != 0)
      unique_profile_ids_.erase(iter->unique_id());
  }

  // Any remaining IDs are not in the new profile list and should be removed
  // from the web database.
  for (std::set<int>::iterator iter = unique_profile_ids_.begin();
       iter != unique_profile_ids_.end(); ++iter) {
    wds->RemoveAutoFillProfile(*iter);

    // Also remove these IDs from the total set of unique IDs.
    unique_ids_.erase(*iter);
  }

  // Clear the unique IDs.  The set of unique IDs is updated for each profile
  // added to |web_profiles_| below.
  unique_profile_ids_.clear();

  // Update the web database with the existing profiles.  We need to handle
  // these first so that |unique_profile_ids_| is reset with the IDs of the
  // existing profiles; otherwise, new profiles added before older profiles can
  // take their unique ID.
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    if (iter->unique_id() != 0) {
      unique_profile_ids_.insert(iter->unique_id());
      wds->UpdateAutoFillProfile(*iter);
    }
  }

  // Add the new profiles to the web database.
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    // The profile was added by the AutoFill dialog, so we need to set the
    // unique ID.  This also means we need to add this profile to the web
    // database.
    if (iter->unique_id() == 0) {
      iter->set_unique_id(CreateNextUniqueIDFor(&unique_profile_ids_));
      wds->AddAutoFillProfile(*iter);
    }
  }

  web_profiles_.reset();
  for (std::vector<AutoFillProfile>::iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    web_profiles_.push_back(new AutoFillProfile(*iter));
  }

  // Read our writes to ensure consistency with the database.
  Refresh();

  FOR_EACH_OBSERVER(Observer, observers_, OnPersonalDataChanged());
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  if (profile_->IsOffTheRecord())
    return;

  // Remove empty credit cards from input.
  credit_cards->erase(
      std::remove_if(
          credit_cards->begin(), credit_cards->end(),
          std::mem_fun_ref(&CreditCard::IsEmpty)),
      credit_cards->end());

  SetUniqueCreditCardLabels(credit_cards);

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!wds)
    return;

  AutoLock lock(unique_ids_lock_);

  // Remove the unique IDs of the new set of credit cards from the unique ID
  // set.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (iter->unique_id() != 0)
      unique_creditcard_ids_.erase(iter->unique_id());
  }

  // Any remaining IDs are not in the new credit card list and should be removed
  // from the web database.
  for (std::set<int>::iterator iter = unique_creditcard_ids_.begin();
       iter != unique_creditcard_ids_.end(); ++iter) {
    wds->RemoveCreditCard(*iter);

    // Also remove these IDs from the total set of unique IDs.
    unique_ids_.erase(*iter);
  }

  // Clear the unique IDs.  The set of unique IDs is updated for each credit
  // card added to |credit_cards_| below.
  unique_creditcard_ids_.clear();

  // Update the web database with the existing credit cards.  We need to handle
  // these first so that |unique_creditcard_ids_| is reset with the IDs of the
  // existing credit cards; otherwise, new credit cards added before older
  // credit cards can take their unique ID.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    if (iter->unique_id() != 0) {
      unique_creditcard_ids_.insert(iter->unique_id());
      wds->UpdateCreditCard(*iter);
    }
  }

  // Add the new credit cards to the web database.
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    // The credit card was added by the AutoFill dialog, so we need to set the
    // unique ID.  This also means we need to add this credit card to the web
    // database.
    if (iter->unique_id() == 0) {
      iter->set_unique_id(CreateNextUniqueIDFor(&unique_creditcard_ids_));
      wds->AddCreditCard(*iter);
    }
  }

  credit_cards_.reset();
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    credit_cards_.push_back(new CreditCard(*iter));
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnPersonalDataChanged());
}

// TODO(jhawkins): Refactor SetProfiles so this isn't so hacky.
void PersonalDataManager::AddProfile(const AutoFillProfile& profile) {
  // Set to true if |profile| is merged into the profile list.
  bool merged = false;

  // Don't save a web profile if the data in the profile is a subset of an
  // auxiliary profile.
  for (std::vector<AutoFillProfile*>::const_iterator iter =
           auxiliary_profiles_.begin();
       iter != auxiliary_profiles_.end(); ++iter) {
    if (profile.IsSubsetOf(**iter))
      return;
  }

  std::vector<AutoFillProfile> profiles;
  for (std::vector<AutoFillProfile*>::const_iterator iter =
           web_profiles_.begin();
       iter != web_profiles_.end(); ++iter) {
    if (profile.IsSubsetOf(**iter)) {
      // In this case, the existing profile already contains all of the data
      // in |profile|, so consider the profiles already merged.
      merged = true;
    } else if ((*iter)->IntersectionOfTypesHasEqualValues(profile)) {
      // |profile| contains all of the data in this profile, plus
      // more.
      merged = true;
      (*iter)->MergeWith(profile);
    }

    profiles.push_back(**iter);
  }

  if (!merged)
    profiles.push_back(profile);

  SetProfiles(&profiles);
}

void PersonalDataManager::UpdateProfile(const AutoFillProfile& profile) {
  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!wds)
    return;

  // Update the cached profile.
  for (std::vector<AutoFillProfile*>::iterator iter = web_profiles_->begin();
       iter != web_profiles_->end(); ++iter) {
    if ((*iter)->unique_id() == profile.unique_id()) {
      delete *iter;
      *iter = new AutoFillProfile(profile);
      break;
    }
  }

  wds->UpdateAutoFillProfile(profile);
  FOR_EACH_OBSERVER(Observer, observers_, OnPersonalDataChanged());
}

void PersonalDataManager::RemoveProfile(int unique_id) {
  // TODO(jhawkins): Refactor SetProfiles so this isn't so hacky.
  std::vector<AutoFillProfile> profiles(web_profiles_.size());
  std::transform(web_profiles_.begin(), web_profiles_.end(),
                 profiles.begin(),
                 DereferenceFunctor<AutoFillProfile>());

  // Remove the profile that matches |unique_id|.
  profiles.erase(
      std::remove_if(profiles.begin(), profiles.end(),
                     FormGroupIDMatchesFunctor<AutoFillProfile>(unique_id)),
      profiles.end());

  SetProfiles(&profiles);
}

// TODO(jhawkins): Refactor SetCreditCards so this isn't so hacky.
void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  std::vector<CreditCard> credit_cards(credit_cards_.size());
  std::transform(credit_cards_.begin(), credit_cards_.end(),
                 credit_cards.begin(),
                 DereferenceFunctor<CreditCard>());

  credit_cards.push_back(credit_card);
  SetCreditCards(&credit_cards);
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!wds)
    return;

  // Update the cached credit card.
  for (std::vector<CreditCard*>::iterator iter = credit_cards_->begin();
       iter != credit_cards_->end(); ++iter) {
    if ((*iter)->unique_id() == credit_card.unique_id()) {
      delete *iter;
      *iter = new CreditCard(credit_card);
      break;
    }
  }

  wds->UpdateCreditCard(credit_card);
  FOR_EACH_OBSERVER(Observer, observers_, OnPersonalDataChanged());
}

void PersonalDataManager::RemoveCreditCard(int unique_id) {
  // TODO(jhawkins): Refactor SetCreditCards so this isn't so hacky.
  std::vector<CreditCard> credit_cards(credit_cards_.size());
  std::transform(credit_cards_.begin(), credit_cards_.end(),
                 credit_cards.begin(),
                 DereferenceFunctor<CreditCard>());

  // Remove the credit card that matches |unique_id|.
  credit_cards.erase(
      std::remove_if(credit_cards.begin(), credit_cards.end(),
                     FormGroupIDMatchesFunctor<CreditCard>(unique_id)),
      credit_cards.end());

  SetCreditCards(&credit_cards);
}

void PersonalDataManager::GetPossibleFieldTypes(const string16& text,
                                                FieldTypeSet* possible_types) {
  string16 clean_info = StringToLowerASCII(CollapseWhitespace(text, false));
  if (clean_info.empty()) {
    possible_types->insert(EMPTY_TYPE);
    return;
  }

  for (ScopedVector<AutoFillProfile>::iterator iter = web_profiles_.begin();
       iter != web_profiles_.end(); ++iter) {
    const FormGroup* profile = *iter;
    if (!profile) {
      DLOG(ERROR) << "NULL information in profiles list";
      continue;
    }

    profile->GetPossibleFieldTypes(clean_info, possible_types);
  }

  for (ScopedVector<CreditCard>::iterator iter = credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    const FormGroup* credit_card = *iter;
    if (!credit_card) {
      DLOG(ERROR) << "NULL information in credit cards list";
      continue;
    }

    credit_card->GetPossibleFieldTypes(clean_info, possible_types);
  }

  if (possible_types->size() == 0)
    possible_types->insert(UNKNOWN_TYPE);
}

bool PersonalDataManager::HasPassword() {
  return !password_hash_.empty();
}

const std::vector<AutoFillProfile*>& PersonalDataManager::profiles() {
  // |profile_| is NULL in AutoFillManagerTest.
  if (!profile_)
    return web_profiles_.get();

  bool auxiliary_profiles_enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled);

#if !defined(OS_MACOSX)
  DCHECK(!auxiliary_profiles_enabled)
      << "Auxiliary profiles supported on Mac only";
#endif

  if (auxiliary_profiles_enabled) {
    profiles_.clear();

    // Populates |auxiliary_profiles_|.
    LoadAuxiliaryProfiles();

    profiles_.insert(profiles_.end(),
        web_profiles_.begin(), web_profiles_.end());
    profiles_.insert(profiles_.end(),
        auxiliary_profiles_.begin(), auxiliary_profiles_.end());
    return profiles_;
  } else {
    return web_profiles_.get();
  }
}

const std::vector<AutoFillProfile*>& PersonalDataManager::web_profiles() {
  return web_profiles_.get();
}

AutoFillProfile* PersonalDataManager::CreateNewEmptyAutoFillProfileForDBThread(
    const string16& label) {
  // See comment in header for thread details.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  AutoLock lock(unique_ids_lock_);
  AutoFillProfile* p = new AutoFillProfile(label,
      CreateNextUniqueIDFor(&unique_profile_ids_));
  return p;
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
}

PersonalDataManager::PersonalDataManager()
    : profile_(NULL),
      is_data_loaded_(false),
      pending_profiles_query_(0),
      pending_creditcards_query_(0) {
}

void PersonalDataManager::Init(Profile* profile) {
  profile_ = profile;
  LoadProfiles();
  LoadCreditCards();
}

int PersonalDataManager::CreateNextUniqueIDFor(std::set<int>* id_set) {
  // Profile IDs MUST start at 1 to allow 0 as an error value when reading
  // the ID from the WebDB (see LoadData()).
  unique_ids_lock_.AssertAcquired();
  int id = 1;
  while (unique_ids_.count(id) != 0)
    ++id;
  unique_ids_.insert(id);
  id_set->insert(id);
  return id;
}

void PersonalDataManager::LoadProfiles() {
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_profiles_query_);

  pending_profiles_query_ = web_data_service->GetAutoFillProfiles(this);
}

// Win and Linux implementations do nothing.  Mac implementation fills in the
// contents of |auxiliary_profiles_|.
#if !defined(OS_MACOSX)
void PersonalDataManager::LoadAuxiliaryProfiles() {
}
#endif

void PersonalDataManager::LoadCreditCards() {
  WebDataService* web_data_service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_creditcards_query_);

  pending_creditcards_query_ = web_data_service->GetCreditCards(this);
}

void PersonalDataManager::ReceiveLoadedProfiles(WebDataService::Handle h,
                                                const WDTypedResult* result) {
  DCHECK_EQ(pending_profiles_query_, h);
  pending_profiles_query_ = 0;

  AutoLock lock(unique_ids_lock_);
  unique_profile_ids_.clear();
  web_profiles_.reset();

  const WDResult<std::vector<AutoFillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutoFillProfile*> >*>(result);

  std::vector<AutoFillProfile*> profiles = r->GetValue();
  for (std::vector<AutoFillProfile*>::iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    unique_profile_ids_.insert((*iter)->unique_id());
    unique_ids_.insert((*iter)->unique_id());
    web_profiles_.push_back(*iter);
  }
}

void PersonalDataManager::ReceiveLoadedCreditCards(
    WebDataService::Handle h, const WDTypedResult* result) {
  DCHECK_EQ(pending_creditcards_query_, h);
  pending_creditcards_query_ = 0;

  AutoLock lock(unique_ids_lock_);
  unique_creditcard_ids_.clear();
  credit_cards_.reset();

  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  for (std::vector<CreditCard*>::iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    unique_creditcard_ids_.insert((*iter)->unique_id());
    unique_ids_.insert((*iter)->unique_id());
    credit_cards_.push_back(*iter);
  }
}

void PersonalDataManager::CancelPendingQuery(WebDataService::Handle* handle) {
  if (*handle) {
    WebDataService* web_data_service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    web_data_service->CancelRequest(*handle);
  }
  *handle = 0;
}

void PersonalDataManager::SetUniqueCreditCardLabels(
    std::vector<CreditCard>* credit_cards) {
  std::map<string16, std::vector<CreditCard*> > label_map;
  for (std::vector<CreditCard>::iterator iter = credit_cards->begin();
       iter != credit_cards->end(); ++iter) {
    label_map[iter->Label()].push_back(&(*iter));
  }

  for (std::map<string16, std::vector<CreditCard*> >::iterator iter =
           label_map.begin();
       iter != label_map.end(); ++iter) {
    // Start at the second element because the first label should not be
    // renamed.  The appended label number starts at 2, because the first label
    // has an implicit index of 1.
    for (size_t i = 1; i < iter->second.size(); ++i) {
      string16 newlabel = iter->second[i]->Label() +
          base::UintToString16(static_cast<unsigned int>(i + 1));
      iter->second[i]->set_label(newlabel);
    }
  }
}

void PersonalDataManager::SaveImportedProfile() {
  if (profile_->IsOffTheRecord())
    return;

  if (!imported_profile_.get())
    return;

  AddProfile(*imported_profile_);
}

// TODO(jhawkins): Refactor and merge this with SaveImportedProfile.
void PersonalDataManager::SaveImportedCreditCard() {
  if (profile_->IsOffTheRecord())
    return;

  if (!imported_credit_card_.get())
    return;

  // Set to true if |imported_credit_card_| is merged into the profile list.
  bool merged = false;

  imported_credit_card_->set_label(ASCIIToUTF16(kUnlabeled));

  std::vector<CreditCard> creditcards;
  for (std::vector<CreditCard*>::const_iterator iter =
           credit_cards_.begin();
       iter != credit_cards_.end(); ++iter) {
    if (imported_credit_card_->IsSubsetOf(**iter)) {
      // In this case, the existing credit card already contains all of the data
      // in |imported_credit_card_|, so consider the credit cards already
      // merged.
      merged = true;
    } else if ((*iter)->IntersectionOfTypesHasEqualValues(
        *imported_credit_card_)) {
      // |imported_profile| contains all of the data in this profile, plus
      // more.
      merged = true;
      (*iter)->MergeWith(*imported_credit_card_);
    }

    creditcards.push_back(**iter);
  }

  if (!merged)
    creditcards.push_back(*imported_credit_card_);

  SetCreditCards(&creditcards);
}
