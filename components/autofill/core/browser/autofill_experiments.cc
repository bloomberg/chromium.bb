// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace autofill {

#if !defined(OS_ANDROID)
const base::Feature kAutofillPrimaryInfoStyleExperiment{
    "AutofillPrimaryInfoStyleExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAutofillPrimaryInfoFontWeightParameterName[] = "font_weight";
const char kAutofillPrimaryInfoFontWeightParameterMedium[] = "medium";
const char kAutofillPrimaryInfoFontWeightParameterSemiBold[] = "semi-bold";
const char kAutofillPrimaryInfoFontWeightParameterBold[] = "bold";
const char kAutofillPrimaryInfoFontWeightParameterExtraBold[] = "extra-bold";

const base::Feature kAutofillDropdownLayoutExperiment{
    "AutofillDropdownLayout", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAutofillDropdownLayoutParameterName[] = "variant";
const char kAutofillDropdownLayoutParameterLeadingIcon[] = "leading-icon";
const char kAutofillDropdownLayoutParameterTrailingIcon[] = "trailing-icon";
#endif  // !defined(OS_ANDROID)

bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const syncer::SyncService* sync_service,
                               const std::string& user_email) {
  // Check Autofill sync setting.
  if (!(sync_service && sync_service->CanSyncStart() &&
        sync_service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE))) {
    return false;
  }

  // Check if the upload to Google state is active.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsInteractionsOnAuthError) &&
      syncer::GetUploadToGoogleState(sync_service,
                                     syncer::ModelType::AUTOFILL_WALLET_DATA) !=
          syncer::UploadState::ACTIVE) {
    return false;
  }

  // Also don't offer upload for users that have a secondary sync passphrase.
  // Users who have enabled a passphrase have chosen to not make their sync
  // information accessible to Google. Since upload makes credit card data
  // available to other Google systems, disable it for passphrase users.
  if (sync_service->IsUsingSecondaryPassphrase())
    return false;

  // Check Payments integration user setting.
  if (!prefs::IsPaymentsIntegrationEnabled(pref_service))
    return false;

  // Check that the user is logged into a supported domain.
  if (user_email.empty())
    return false;

  std::string domain = gaia::ExtractDomainName(user_email);
  // If the "allow all email domains" flag is off, restrict credit card upload
  // only to Google Accounts with @googlemail, @gmail, @google, or @chromium
  // domains.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUpstreamAllowAllEmailDomains) &&
      !(domain == "googlemail.com" || domain == "gmail.com" ||
        domain == "google.com" || domain == "chromium.org")) {
    return false;
  }

  return base::FeatureList::IsEnabled(features::kAutofillUpstream);
}

bool IsInAutofillSuggestionsDisabledExperiment() {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillEnabled");
  return group_name == "Disabled";
}

bool IsAutofillCreditCardAssistEnabled() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(features::kAutofillCreditCardAssist);
#endif
}

features::LocalCardMigrationExperimentalFlag
GetLocalCardMigrationExperimentalFlag() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillCreditCardLocalCardMigration))
    return features::LocalCardMigrationExperimentalFlag::kMigrationDisabled;

  std::string param = base::GetFieldTrialParamValueByFeature(
      features::kAutofillCreditCardLocalCardMigration,
      features::kAutofillCreditCardLocalCardMigrationParameterName);

  if (param ==
      features::
          kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage) {
    return features::LocalCardMigrationExperimentalFlag::
        kMigrationWithoutSettingsPage;
  }
  return features::LocalCardMigrationExperimentalFlag::
      kMigrationIncludeSettingsPage;
}

bool IsAutofillNoLocalSaveOnUploadSuccessExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillNoLocalSaveOnUploadSuccess);
}

bool OfferStoreUnmaskedCards() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // The checkbox can be forced on with a flag, but by default we don't store
  // on Linux due to lack of system keychain integration. See crbug.com/162735
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableOfferStoreUnmaskedWalletCards);
#else
  // Query the field trial before checking command line flags to ensure UMA
  // reports the correct group.
  std::string group_name =
      base::FieldTrialList::FindFullName("OfferStoreUnmaskedWalletCards");

  // The checkbox can be forced on or off with flags.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfferStoreUnmaskedWalletCards))
    return true;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfferStoreUnmaskedWalletCards))
    return false;

  // Otherwise use the field trial to show the checkbox or not.
  return group_name != "Disabled";
#endif
}

bool ShouldUseActiveSignedInAccount() {
  // If butter is enabled or the feature to get the Payment Identity from Sync
  // is enabled, the account of the active signed-in user should be used.
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableAccountWalletStorage) ||
         base::FeatureList::IsEnabled(
             features::kAutofillGetPaymentsIdentityFromSync);
}

#if !defined(OS_ANDROID)
namespace {

// Returns the font weight corresponding to the value of param
// kAutofillPrimaryInfoFontWeightParameterName, or INVALID if the param is not
// valid.
gfx::Font::Weight GetFontWeightFromParam() {
  std::string param = base::GetFieldTrialParamValueByFeature(
      autofill::kAutofillPrimaryInfoStyleExperiment,
      autofill::kAutofillPrimaryInfoFontWeightParameterName);

  if (param == autofill::kAutofillPrimaryInfoFontWeightParameterMedium)
    return gfx::Font::Weight::MEDIUM;
  if (param == autofill::kAutofillPrimaryInfoFontWeightParameterSemiBold)
    return gfx::Font::Weight::SEMIBOLD;
  if (param == autofill::kAutofillPrimaryInfoFontWeightParameterBold)
    return gfx::Font::Weight::BOLD;
  if (param == autofill::kAutofillPrimaryInfoFontWeightParameterExtraBold)
    return gfx::Font::Weight::EXTRA_BOLD;

  return gfx::Font::Weight::INVALID;
}

}  // namespace

bool ShouldUseCustomFontWeightForPrimaryInfo(gfx::Font::Weight* font_weight) {
  if (!base::FeatureList::IsEnabled(
          autofill::kAutofillPrimaryInfoStyleExperiment)) {
    return false;
  }

  // Only read the feature param's value the first time it's needed.
  static gfx::Font::Weight font_weight_from_param = GetFontWeightFromParam();
  if (font_weight_from_param == gfx::Font::Weight::INVALID)
    return false;

  *font_weight = font_weight_from_param;
  return true;
}

ForcedPopupLayoutState GetForcedPopupLayoutState() {
  if (!base::FeatureList::IsEnabled(
          autofill::kAutofillDropdownLayoutExperiment))
    return ForcedPopupLayoutState::kDefault;

  std::string param = base::GetFieldTrialParamValueByFeature(
      autofill::kAutofillDropdownLayoutExperiment,
      autofill::kAutofillDropdownLayoutParameterName);

  if (param == autofill::kAutofillDropdownLayoutParameterLeadingIcon) {
    return ForcedPopupLayoutState::kLeadingIcon;
  } else if (param == autofill::kAutofillDropdownLayoutParameterTrailingIcon) {
    return ForcedPopupLayoutState::kTrailingIcon;
  }

  // Unknown parameter value.
  NOTREACHED();
  return ForcedPopupLayoutState::kDefault;
}
#endif  // !defined(OS_ANDROID)

}  // namespace autofill
