// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_pref_names.h"
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

const base::Feature kAutofillAlwaysFillAddresses{
    "AlwaysFillAddresses", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillCreateDataForTest{
    "AutofillCreateDataForTest", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillCreditCardAssist{
    "AutofillCreditCardAssist", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillScanCardholderName{
    "AutofillScanCardholderName", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillCreditCardAblationExperiment{
    "AutofillCreditCardAblationExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillCreditCardLocalCardMigration{
    "AutofillCreditCardLocalCardMigration", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillDeleteDisusedAddresses{
    "AutofillDeleteDisusedAddresses", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillDeleteDisusedCreditCards{
    "AutofillDeleteDisusedCreditCards", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillExpandedPopupViews{
    "AutofillExpandedPopupViews", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillPreferServerNamePredictions{
    "AutofillPreferServerNamePredictions", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillRationalizeFieldTypePredictions{
    "AutofillRationalizeFieldTypePredictions",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillSaveCardDialogUnlabeledExpirationDate{
    "AutofillSaveCardDialogUnlabeledExpirationDate",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillSuggestInvalidProfileData{
    "AutofillSuggestInvalidProfileData", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillSuppressDisusedAddresses{
    "AutofillSuppressDisusedAddresses", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillSuppressDisusedCreditCards{
    "AutofillSuppressDisusedCreditCards", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillUpstream{"AutofillUpstream",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillUpstreamAllowAllEmailDomains{
    "AutofillUpstreamAllowAllEmailDomains", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillUpstreamAlwaysRequestCardholderName{
    "AutofillUpstreamAlwaysRequestCardholderName",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillUpstreamBlankCardholderNameField{
    "AutofillUpstreamBlankCardholderNameField",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillUpstreamEditableCardholderName{
    "AutofillUpstreamEditableCardholderName",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillUpstreamSendPanFirstSix{
    "AutofillUpstreamSendPanFirstSix", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kAutofillUpstreamUpdatePromptExplanation{
    "AutofillUpstreamUpdatePromptExplanation",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kAutofillVoteUsingInvalidProfileData{
    "AutofillVoteUsingInvalidProfileData", base::FEATURE_ENABLED_BY_DEFAULT};

const char kCreditCardSigninPromoImpressionLimitParamKey[] = "impression_limit";

#if defined(OS_MACOSX)
const base::Feature kMacViewsAutofillPopup{"MacViewsAutofillPopup",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

bool IsAutofillEnabled(const PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kAutofillEnabled);
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
  return base::FeatureList::IsEnabled(kAutofillCreditCardAssist);
#endif
}

bool IsAutofillCreditCardLocalCardMigrationExperimentEnabled() {
  return base::FeatureList::IsEnabled(kAutofillCreditCardLocalCardMigration);
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
  if (!pref_service->GetBoolean(prefs::kAutofillWalletImportEnabled))
    return false;

  // Check that the user is logged into a supported domain.
  if (user_email.empty())
    return false;
  std::string domain = gaia::ExtractDomainName(user_email);
  // If the "allow all email domains" flag is off, restrict credit card upload
  // only to Google Accounts with @googlemail, @gmail, @google, or @chromium
  // domains.
  if (!base::FeatureList::IsEnabled(kAutofillUpstreamAllowAllEmailDomains) &&
      !(domain == "googlemail.com" || domain == "gmail.com" ||
        domain == "google.com" || domain == "chromium.org")) {
    return false;
  }

  return base::FeatureList::IsEnabled(kAutofillUpstream);
}

bool IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      kAutofillUpstreamAlwaysRequestCardholderName);
}

bool IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      kAutofillUpstreamBlankCardholderNameField);
}

bool IsAutofillUpstreamEditableCardholderNameExperimentEnabled() {
  return base::FeatureList::IsEnabled(kAutofillUpstreamEditableCardholderName);
}

bool IsAutofillUpstreamSendPanFirstSixExperimentEnabled() {
  return base::FeatureList::IsEnabled(kAutofillUpstreamSendPanFirstSix);
}

bool IsAutofillUpstreamUpdatePromptExplanationExperimentEnabled() {
  return base::FeatureList::IsEnabled(kAutofillUpstreamUpdatePromptExplanation);
}

#if defined(OS_MACOSX)
bool IsMacViewsAutofillPopupExperimentEnabled() {
#if BUILDFLAG(MAC_VIEWS_BROWSER)
  if (!::features::IsViewsBrowserCocoa())
    return true;
#endif

  return base::FeatureList::IsEnabled(kMacViewsAutofillPopup);
}
#endif  // defined(OS_MACOSX)

bool ShouldUseNativeViews() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  return base::FeatureList::IsEnabled(kAutofillExpandedPopupViews) ||
         base::FeatureList::IsEnabled(::features::kExperimentalUi);
#else
  return false;
#endif
}

bool IsAutofillSaveCardDialogUnlabeledExpirationDateEnabled() {
  return base::FeatureList::IsEnabled(
      kAutofillSaveCardDialogUnlabeledExpirationDate);
}

}  // namespace autofill
