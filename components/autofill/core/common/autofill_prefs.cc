// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace autofill {
namespace prefs {

// Integer that is set to the last choice user made when prompted for saving a
// credit card. The prompt is for user's consent in saving the card in the
// server for signed in users and saving the card locally for non signed-in
// users.
const char kAutofillAcceptSaveCreditCardPromptState[] =
    "autofill.accept_save_credit_card_prompt_state";

// Integer that is set to the billing customer number fetched from priority
// preference.
const char kAutofillBillingCustomerNumber[] = "billing_customer_number";

// The field type, validity state map of all profiles.
const char kAutofillProfileValidity[] = "autofill.profile_validity";

// Boolean that is true if Autofill is enabled and allowed to save credit card
// data.
const char kAutofillCreditCardEnabled[] = "autofill.credit_card_enabled";

// Number of times the credit card signin promo has been shown.
const char kAutofillCreditCardSigninPromoImpressionCount[] =
    "autofill.credit_card_signin_promo_impression_count";

// Boolean that is true if Autofill is enabled and allowed to save data.
const char kAutofillEnabledDeprecated[] = "autofill.enabled";

// Boolean that is true if Japan address city field has been migrated to be a
// part of the street field.
const char kAutofillJapanCityFieldMigrated[] =
    "autofill.japan_city_field_migrated_to_street_address";

// Integer that is set to the last version where the profile deduping routine
// was run. This routine will be run once per version.
const char kAutofillLastVersionDeduped[] = "autofill.last_version_deduped";

// Integer that is set to the last version where disused addresses were
// deleted. This deletion will be run once per version.
const char kAutofillLastVersionDisusedAddressesDeleted[] =
    "autofill.last_version_disused_addresses_deleted";

// Integer that is set to the last version where disused credit cards were
// deleted. This deletion will be run once per version.
const char kAutofillLastVersionDisusedCreditCardsDeleted[] =
    "autofill.last_version_disused_credit_cards_deleted";

// Boolean that is true if the orphan rows in the autofill table were removed.
const char kAutofillOrphanRowsRemoved[] = "autofill.orphan_rows_removed";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillProfileEnabled[] = "autofill.profile_enabled";

// Boolean that's true when Wallet card and address import is enabled by the
// user.
const char kAutofillWalletImportEnabled[] = "autofill.wallet_import_enabled";

// Boolean that is set to the last choice user made when prompted for saving an
// unmasked server card locally.
const char kAutofillWalletImportStorageCheckboxState[] =
    "autofill.wallet_import_storage_checkbox_state";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDoublePref(
      prefs::kAutofillBillingCustomerNumber, 0.0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // This pref is not synced because it's for a signin promo, which by
  // definition will not be synced.
  registry->RegisterIntegerPref(
      prefs::kAutofillCreditCardSigninPromoImpressionCount, 0);
  registry->RegisterBooleanPref(
      prefs::kAutofillEnabledDeprecated, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillProfileEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kAutofillJapanCityFieldMigrated, false);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedAddressesDeleted, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // These choices are made on a per-device basis, so they're not syncable.
  registry->RegisterBooleanPref(prefs::kAutofillWalletImportEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletImportStorageCheckboxState, true);
  registry->RegisterIntegerPref(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDisusedCreditCardsDeleted, 0);
  registry->RegisterBooleanPref(prefs::kAutofillCreditCardEnabled, true);
  registry->RegisterBooleanPref(prefs::kAutofillOrphanRowsRemoved, false);
  registry->RegisterStringPref(
      prefs::kAutofillProfileValidity, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

bool IsAutocompleteEnabled(const PrefService* prefs) {
  return IsProfileAutofillEnabled(prefs);
}

bool IsAutofillEnabled(const PrefService* prefs) {
  return IsProfileAutofillEnabled(prefs) || IsCreditCardAutofillEnabled(prefs);
}

void SetAutofillEnabled(PrefService* prefs, bool enabled) {
  SetProfileAutofillEnabled(prefs, enabled);
  SetCreditCardAutofillEnabled(prefs, enabled);
}

bool IsAutofillManaged(const PrefService* prefs) {
  return prefs->IsManagedPreference(kAutofillEnabledDeprecated);
}

bool IsProfileAutofillEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillProfileEnabled);
}

void SetProfileAutofillEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillProfileEnabled, enabled);
}

bool IsCreditCardAutofillEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillCreditCardEnabled);
}

void SetCreditCardAutofillEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillCreditCardEnabled, enabled);
}

bool IsPaymentsIntegrationEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(kAutofillWalletImportEnabled);
}

void SetPaymentsIntegrationEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kAutofillWalletImportEnabled, enabled);
}

std::string GetAllProfilesValidityMapsEncodedString(const PrefService* prefs) {
  return prefs->GetString(kAutofillProfileValidity);
}

}  // namespace prefs
}  // namespace autofill
