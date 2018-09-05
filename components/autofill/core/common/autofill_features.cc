// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace autofill {
namespace features {

// Controls whether the AddressNormalizer is supplied. If available, it may be
// used to normalize address and will incur fetching rules from the server.
const base::Feature kAutofillAddressNormalizer{
    "AutofillAddressNormalizer", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillAlwaysFillAddresses{
    "AlwaysFillAddresses", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the use of GET (instead of POST) to fetch cacheable autofill query
// responses.
const base::Feature kAutofillCacheQueryResponses{
    "AutofillCacheQueryResponses", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillCreateDataForTest{
    "AutofillCreateDataForTest", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardAblationExperiment{
    "AutofillCreditCardAblationExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardAssist{
    "AutofillCreditCardAssist", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardLocalCardMigration{
    "AutofillCreditCardLocalCardMigration", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillDeleteDisusedAddresses{
    "AutofillDeleteDisusedAddresses", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillDeleteDisusedCreditCards{
    "AutofillDeleteDisusedCreditCards", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the credit card downstream keyboard accessory shows
// the Google Pay logo animation on iOS.
const base::Feature kAutofillDownstreamUseGooglePayBrandingOniOS{
    "AutofillDownstreamUseGooglePayBrandingOniOS",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill attemps to fill dynamically changing forms.
const base::Feature kAutofillDynamicForms{"AutofillDynamicForms",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the server credit cards are saved only in the ephemeral
// account-based storage, instead of the persistent local storage.
const base::Feature kAutofillEnableAccountWalletStorage{
    "AutofillEnableAccountWalletStorage", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillEnableCompanyName{
    "AutofillEnableCompanyName", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the server credit cards are offered to be filled and
// uploaded to Google Pay if the sync service is in auth error.
const base::Feature kAutofillEnablePaymentsInteractionsOnAuthError{
    "AutofillDontOfferServerCardsOnAuthError",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, no local copy of server card will be saved when upload
// succeeds.
const base::Feature kAutofillNoLocalSaveOnUploadSuccess{
    "AutofillNoLocalSaveOnUploadSuccess", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// heuristic field type prediction is run for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics{
    "AutofillEnforceMinRequiredFieldsForHeuristics",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// crowd-sourced field type predictions are queried for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForQuery{
    "AutofillEnforceMinRequiredFieldsForQuery",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// field type votes are uploaded to the crowd-sourcing server.
const base::Feature kAutofillEnforceMinRequiredFieldsForUpload{
    "AutofillEnforceMinRequiredFieldsForUpload",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillExpandedPopupViews{
    "AutofillExpandedPopupViews", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, gets payment identity from sync service instead of
// identity manager.
const base::Feature kAutofillGetPaymentsIdentityFromSync{
    "AutofillGetPaymentsIdentityFromSync", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill Local Card Migration will show result dialogs.
const base::Feature kAutofillLocalCardMigrationShowFeedback{
    "AutofillLocalCardMigrationShowFeedback",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the manual fill fallback will be present.
const base::Feature kAutofillManualFallback{"AutofillManualFallback",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillPreferServerNamePredictions{
    "AutofillPreferServerNamePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill should fill fields previously filled by the
// website.
const base::Feature kAutofillPrefilledFields{"AutofillPrefilledFields",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillRationalizeFieldTypePredictions{
    "AutofillRationalizeFieldTypePredictions",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether Autofill should rationalize repeated server type
// predictions.
const base::Feature kAutofillRationalizeRepeatedServerPredictions{
    "kAutofillRationalizeRepeatedServerPredictions",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether Full Server credit cards should be reset when the sync
// service is in an auth error state.
const base::Feature kAutofillResetFullServerCardsOnAuthError{
    "AutofillResetFullServerCardsOnAuthError",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a group of fields not enclosed in a form can be
// considered a form. If this is enabled, unowned fields will only constitute
// a form if there are signals to suggest that this might a checkout page.
const base::Feature kAutofillRestrictUnownedFieldsToFormlessCheckout{
    "AutofillRestrictUnownedFieldsToFormlessCheckout",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillSaveCardDialogUnlabeledExpirationDate{
    "AutofillSaveCardDialogUnlabeledExpirationDate",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, a sign in promo will show up right after the user
// saves a card locally. This also introduces a "Manage Cards" bubble.
const base::Feature kAutofillSaveCardSignInAfterLocalSave{
    "AutofillSaveCardSignInAfterLocalSave", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillScanCardholderName{
    "AutofillScanCardholderName", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether experiment ids should be sent through
// Google Payments RPCs or not.
const base::Feature kAutofillSendExperimentIdsInPaymentsRPCs{
    "AutofillSendExperimentIdsInPaymentsRPCs",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, only countries of recently-used addresses are sent in the
// GetUploadDetails call to Payments. If disabled, whole recently-used addresses
// are sent.
const base::Feature kAutofillSendOnlyCountryInGetUploadDetails{
    "AutofillSendOnlyCountryInGetUploadDetails",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
const base::Feature kAutofillServerCommunication{
    "kAutofillServerCommunication", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether autofill suggestions are filtered by field values previously
// filled by website.
const base::Feature kAutofillShowAllSuggestionsOnPrefilledForms{
    "AutofillShowAllSuggestionsOnPrefilledForms",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show warnings in the Dev console for misused autocomplete
// types.
const base::Feature kAutofillShowAutocompleteConsoleWarnings{
    "AutofillShowAutocompleteConsoleWarnings",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
const base::Feature kAutofillShowTypePredictions{
    "AutofillShowTypePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether inferred label is considered for comparing in
// FormFieldData.SimilarFieldAs.
const base::Feature kAutofillSkipComparingInferredLabels{
    "AutofillSkipComparingInferredLabels", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Controls whether ELO cards should be uploaded to Google Payments.
const base::Feature kAutofillUpstreamDisallowElo{
    "AutofillUpstreamDisallowElo", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether JCB cards should be uploaded to Google Payments.
const base::Feature kAutofillUpstreamDisallowJcb{
    "AutofillUpstreamDisallowJcb", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamEditableCardholderName{
    "AutofillUpstreamEditableCardholderName",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamSendPanFirstSix{
    "AutofillUpstreamSendPanFirstSix", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamUpdatePromptExplanation{
    "AutofillUpstreamUpdatePromptExplanation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the credit card upload bubble shows the Google Pay logo and
// a shorter "Save card?" header message on mobile.
const base::Feature kAutofillUpstreamUseGooglePayBrandingOnMobile{
    "AutofillUpstreamUseGooglePayOnAndroidBranding",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the PaymentsCustomerData is used to make requests to
// Google Payments.
const base::Feature kAutofillUsePaymentsCustomerData{
    "AutofillUsePaymentsCustomerData", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillVoteUsingInvalidProfileData{
    "AutofillVoteUsingInvalidProfileData", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether password generation is offered automatically on fields
// percieved as eligible for generation.
const base::Feature kAutomaticPasswordGeneration = {
    "AutomaticPasswordGeneration", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not the autofill UI triggers on a single click.
const base::Feature kSingleClickAutofill{"SingleClickAutofill",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const char kAutofillLocalCardMigrationCloseButtonDelay[] =
    "show_close_migration_dialog_button_delay";

const char kAutofillCreditCardLocalCardMigrationParameterName[] = "variant";

const char kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage[] =
    "without-settings-page";

const char kCreditCardSigninPromoImpressionLimitParamKey[] = "impression_limit";

#if defined(OS_ANDROID)
// Controls whether to use modernized style for the Autofill dropdown.
const base::Feature kAutofillRefreshStyleAndroid{
    "AutofillRefreshStyleAndroid", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_ANDROID

#if defined(OS_MACOSX)
const base::Feature kMacViewsAutofillPopup{"MacViewsAutofillPopup",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_MACOSX)

bool IsAutofillCreditCardAssistEnabled() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(kAutofillCreditCardAssist);
#endif
}

LocalCardMigrationExperimentalFlag GetLocalCardMigrationExperimentalFlag() {
  if (!base::FeatureList::IsEnabled(kAutofillCreditCardLocalCardMigration))
    return LocalCardMigrationExperimentalFlag::kMigrationDisabled;

  std::string param = base::GetFieldTrialParamValueByFeature(
      kAutofillCreditCardLocalCardMigration,
      kAutofillCreditCardLocalCardMigrationParameterName);

  if (param ==
      kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage) {
    return LocalCardMigrationExperimentalFlag::kMigrationWithoutSettingsPage;
  }
  return LocalCardMigrationExperimentalFlag::kMigrationIncludeSettingsPage;
}

base::TimeDelta GetTimeoutForMigrationPromptFeedbackCloseButton() {
  constexpr int show_close_button_timeout_in_seconds = 5;
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      kAutofillCreditCardLocalCardMigration,
      kAutofillLocalCardMigrationCloseButtonDelay,
      show_close_button_timeout_in_seconds));
}

bool IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillUpstreamAlwaysRequestCardholderName);
}

bool IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillUpstreamBlankCardholderNameField);
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

}  // namespace features
}  // namespace autofill
