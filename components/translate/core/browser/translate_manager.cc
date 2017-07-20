// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_experiment.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"

namespace translate {

const base::Feature kTranslateLanguageByULP{"TranslateLanguageByULP",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
namespace {

// Callbacks for translate errors.
TranslateManager::TranslateErrorCallbackList* g_callback_list_ = NULL;

const char kReportLanguageDetectionErrorURL[] =
    "https://translate.google.com/translate_error?client=cr&action=langidc";

// Used in kReportLanguageDetectionErrorURL to specify the original page
// language.
const char kSourceLanguageQueryName[] = "sl";

// Used in kReportLanguageDetectionErrorURL to specify the page URL.
const char kUrlQueryName[] = "u";

// Name for params in config for considering ULP in GetTargetLanguage().
const char kTargetLanguageULPConfidenceThresholdName[] =
    "target_language_ulp_confidence_threshold";
const char kTargetLanguageULPProbabilityThresholdName[] =
    "target_language_ulp_probability_threshold";

// Name for params in config for considering ULP in InitiateTranslation().
const char kInitiateTranslationULPConfidenceThresholdName[] =
    "initiate_translation_ulp_confidence_threshold";
const char kInitiateTranslationULPProbabilityThresholdName[] =
    "initiate_translation_ulp_probability_threshold";

// Constants for considering ULP. These built-in constatants of default will be
// override by the value in config params if present.
//   Default constants for the GetTargetLanguage() function:
//     The confidence threshold that we will consider to use the ULP
//     "reading list".
const double kDefaultTargetLanguageULPConfidenceThreshold = 0.7;
//     The probability threshold that we will consider to use a language on
//     ULP "reading list".
const double kDefaultTargetLanguageULPProbabilityThreshold = 0.55;

//   Default constants for the InitiateTranslation() function:
//     The confidence threshold that we will consider to use the ULP
//     "reading list".
const double kDefaultInitiateTranslationULPConfidenceThreshold = 0.75;
//     The probability threshold that we will consider to use a language on
//     ULP "reading list".
const double kDefaultInitiateTranslationULPProbabilityThreshold = 0.5;

// Return the probability of the |language| in the |list|, or 0.0 if it is not
// in
// the |list|.
double GetLanguageProbability(
    const TranslatePrefs::LanguageAndProbabilityList& list,
    const std::string language) {
  for (const auto& it : list) {
    if (language == it.first) {
      return it.second;
    }
  }
  return 0.0;
}

// Get a value from the |map| by |key| and return the converted double, if
// failed
// return the |default_value| instead.
double GetDoubleFromMap(std::map<std::string, std::string>& map,
                        const std::string& key,
                        double default_value) {
  double value = default_value;
  return base::StringToDouble(map[key], &value) ? value : default_value;
}

}  // namespace

TranslateManager::~TranslateManager() {}

// static
std::unique_ptr<TranslateManager::TranslateErrorCallbackList::Subscription>
TranslateManager::RegisterTranslateErrorCallback(
    const TranslateManager::TranslateErrorCallback& callback) {
  if (!g_callback_list_)
    g_callback_list_ = new TranslateErrorCallbackList;
  return g_callback_list_->Add(callback);
}

TranslateManager::TranslateManager(
    TranslateClient* translate_client,
    TranslateRanker* translate_ranker,
    const std::string& accept_languages_pref_name)
    : page_seq_no_(0),
      accept_languages_pref_name_(accept_languages_pref_name),
      translate_client_(translate_client),
      translate_driver_(translate_client_->GetTranslateDriver()),
      translate_ranker_(translate_ranker),
      language_state_(translate_driver_),
      translate_event_(base::MakeUnique<metrics::TranslateEventProto>()),
      weak_method_factory_(this) {}

base::WeakPtr<TranslateManager> TranslateManager::GetWeakPtr() {
  return weak_method_factory_.GetWeakPtr();
}

void TranslateManager::InitiateTranslation(const std::string& page_lang) {
  // TODO(rogerm): Remove ScopedTracker below once crbug.com/646711 is closed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "646711 translate::TranslateManager::InitiateTranslation"));

  // Short-circuit out if not in a state where initiating translation makes
  // sense (this method may be called muhtiple times for a given page).
  if (!language_state_.page_needs_translation() ||
      language_state_.translation_pending() ||
      language_state_.translation_declined() ||
      language_state_.IsPageTranslated()) {
    return;
  }

  // Also, skip if the connection is currently offline - initiation doesn't make
  // sense there, either.
  if (net::NetworkChangeNotifier::IsOffline())
    return;

  if (!ignore_missing_key_for_testing_ && !::google_apis::HasKeysConfigured()) {
    // Without an API key, translate won't work, so don't offer to translate
    // in the first place. Leave prefs::kEnableTranslate on, though, because
    // that settings syncs and we don't want to turn off translate everywhere
    // else.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_KEY);
    return;
  }

  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());

  if (!translate_prefs->IsEnabled()) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS);
    RecordTranslateEvent(metrics::TranslateEventProto::DISABLED_BY_PREF);
    const std::string& locale =
        TranslateDownloadManager::GetInstance()->application_locale();
    TranslateBrowserMetrics::ReportLocalesOnDisabledByPrefs(locale);
    return;
  }

  // MHTML pages currently cannot be translated.
  // See bug: 217945.
  if (translate_driver_->GetContentsMimeType() == "multipart/related") {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED);
    return;
  }

  // Don't translate any Chrome specific page, e.g., New Tab Page, Download,
  // History, and so on.
  const GURL& page_url = translate_driver_->GetVisibleURL();
  if (!translate_client_->IsTranslatableURL(page_url)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED);
    return;
  }

  std::string target_lang = GetTargetLanguage(translate_prefs.get());
  std::string language_code =
      TranslateDownloadManager::GetLanguageCode(page_lang);

  InitTranslateEvent(language_code, target_lang, *translate_prefs);

  // Don't translate similar languages (ex: en-US to en).
  if (language_code == target_lang) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES);
    return;
  }

  // Querying the ranker now, but not exiting immediately so that we may log
  // other potential suppression reasons.
  bool should_offer_translation =
      translate_ranker_->ShouldOfferTranslation(translate_event_.get());

  // Nothing to do if either the language Chrome is in or the language of
  // the page is not supported by the translation server.
  if (target_lang.empty() ||
      !TranslateDownloadManager::IsSupportedLanguage(language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED);
    TranslateBrowserMetrics::ReportUnsupportedLanguageAtInitiation(
        language_code);
    RecordTranslateEvent(metrics::TranslateEventProto::UNSUPPORTED_LANGUAGE);
    return;
  }

  TranslateAcceptLanguages* accept_languages =
      translate_client_->GetTranslateAcceptLanguages();
  // Don't translate any user black-listed languages.
  if (!translate_prefs->CanTranslateLanguage(accept_languages, language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
    RecordTranslateEvent(
        metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_USER_CONFIG);
    return;
  }

  // Don't translate any user black-listed URLs.
  if (translate_prefs->IsSiteBlacklisted(page_url.HostNoBrackets())) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
    RecordTranslateEvent(
        metrics::TranslateEventProto::URL_DISABLED_BY_USER_CONFIG);
    return;
  }

  // If the user has previously selected "always translate" for this language we
  // automatically translate.  Note that in incognito mode we disable that
  // feature; the user will get an infobar, so they can control whether the
  // page's text is sent to the translate server.
  if (!translate_driver_->IsIncognito()) {
    std::string auto_target_lang =
        GetAutoTargetLanguage(language_code, translate_prefs.get());
    if (!auto_target_lang.empty()) {
      TranslateBrowserMetrics::ReportInitiationStatus(
          TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_CONFIG);
      translate_event_->set_modified_target_language(auto_target_lang);
      RecordTranslateEvent(
          metrics::TranslateEventProto::AUTO_TRANSLATION_BY_PREF);
      TranslatePage(language_code, auto_target_lang, false);
      return;
    }
  }

  std::string auto_translate_to = language_state_.AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_LINK);
    translate_event_->set_modified_target_language(auto_translate_to);
    RecordTranslateEvent(
        metrics::TranslateEventProto::AUTO_TRANSLATION_BY_LINK);
    TranslatePage(language_code, auto_translate_to, false);
    return;
  }

  if (LanguageInULP(language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IN_ULP);
    RecordTranslateEvent(metrics::TranslateEventProto::LANGUAGE_IN_ULP);
    return;
  }

  if (!should_offer_translation) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_ABORTED_BY_RANKER);
    RecordTranslateEvent(metrics::TranslateEventProto::DISABLED_BY_RANKER);
    return;
  }

  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_SHOW_INFOBAR);

  // Prompts the user if they want the page translated.
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
                                     language_code, target_lang,
                                     TranslateErrors::NONE, false);
}

bool TranslateManager::LanguageInULP(const std::string& language) const {
  if (!base::FeatureList::IsEnabled(kTranslateLanguageByULP))
    return false;
  std::map<std::string, std::string> params;
  variations::GetVariationParamsByFeature(translate::kTranslateLanguageByULP,
                                          &params);
  // Check the language & probability on the reading list.
  TranslatePrefs::LanguageAndProbabilityList reading;
  if (translate_client_->GetTranslatePrefs()->GetReadingFromUserLanguageProfile(
          &reading) >
          GetDoubleFromMap(params,
                           kInitiateTranslationULPConfidenceThresholdName,
                           kDefaultInitiateTranslationULPConfidenceThreshold) &&
      GetLanguageProbability(reading, language) >
          GetDoubleFromMap(params,
                           kInitiateTranslationULPProbabilityThresholdName,
                           kDefaultInitiateTranslationULPProbabilityThreshold))
    return true;
  return false;
}

void TranslateManager::TranslatePage(const std::string& original_source_lang,
                                     const std::string& target_lang,
                                     bool triggered_from_menu) {
  if (!translate_driver_->HasCurrentPage()) {
    NOTREACHED();
    return;
  }

  // Log the source and target languages of the translate request.
  TranslateBrowserMetrics::ReportTranslateSourceLanguage(original_source_lang);
  TranslateBrowserMetrics::ReportTranslateTargetLanguage(target_lang);

  // Translation can be kicked by context menu against unsupported languages.
  // Unsupported language strings should be replaced with
  // kUnknownLanguageCode in order to send a translation request with enabling
  // server side auto language detection.
  std::string source_lang(original_source_lang);
  if (!TranslateDownloadManager::IsSupportedLanguage(source_lang))
    source_lang = std::string(translate::kUnknownLanguageCode);

  // Capture the translate event if we were triggered from the menu.
  if (triggered_from_menu) {
    RecordTranslateEvent(
        metrics::TranslateEventProto::USER_CONTEXT_MENU_TRANSLATE);
  }

  // Trigger the "translating now" UI.
  translate_client_->ShowTranslateUI(
      translate::TRANSLATE_STEP_TRANSLATING, source_lang, target_lang,
      TranslateErrors::NONE, triggered_from_menu);

  TranslateScript* script = TranslateDownloadManager::GetInstance()->script();
  DCHECK(script != NULL);

  const std::string& script_data = script->data();
  if (!script_data.empty()) {
    DoTranslatePage(script_data, source_lang, target_lang);
    return;
  }

  // The script is not available yet.  Queue that request and query for the
  // script.  Once it is downloaded we'll do the translate.
  TranslateScript::RequestCallback callback =
      base::Bind(&TranslateManager::OnTranslateScriptFetchComplete,
                 GetWeakPtr(), source_lang, target_lang);

  script->Request(callback);
}

void TranslateManager::RevertTranslation() {
  // Capture the revert event in the translate metrics
  RecordTranslateEvent(metrics::TranslateEventProto::USER_REVERT);

  // Revert the translation.
  translate_driver_->RevertTranslation(page_seq_no_);
  language_state_.SetCurrentLanguage(language_state_.original_language());
}

void TranslateManager::ReportLanguageDetectionError() {
  TranslateBrowserMetrics::ReportLanguageDetectionError();

  GURL report_error_url = GURL(kReportLanguageDetectionErrorURL);

  report_error_url = net::AppendQueryParameter(
      report_error_url, kUrlQueryName,
      translate_driver_->GetLastCommittedURL().spec());

  report_error_url =
      net::AppendQueryParameter(report_error_url, kSourceLanguageQueryName,
                                language_state_.original_language());

  report_error_url = translate::AddHostLocaleToUrl(report_error_url);
  report_error_url = translate::AddApiKeyToUrl(report_error_url);

  translate_client_->ShowReportLanguageDetectionErrorUI(report_error_url);
}

void TranslateManager::DoTranslatePage(const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  language_state_.set_translation_pending(true);
  translate_driver_->TranslatePage(page_seq_no_, translate_script, source_lang,
                                   target_lang);
}

// Notifies |g_callback_list_| of translate errors.
void TranslateManager::NotifyTranslateError(TranslateErrors::Type error_type) {
  if (!g_callback_list_ || error_type == TranslateErrors::NONE ||
      translate_driver_->IsIncognito()) {
    return;
  }

  TranslateErrorDetails error_details;
  error_details.time = base::Time::Now();
  error_details.url = translate_driver_->GetLastCommittedURL();
  error_details.error = error_type;
  g_callback_list_->Notify(error_details);
}

void TranslateManager::PageTranslated(const std::string& source_lang,
                                      const std::string& target_lang,
                                      TranslateErrors::Type error_type) {
  if (error_type == TranslateErrors::NONE)
    language_state_.SetCurrentLanguage(target_lang);

  language_state_.set_translation_pending(false);
  language_state_.set_translation_error(error_type != TranslateErrors::NONE);

  if ((error_type == TranslateErrors::NONE) &&
      source_lang != translate::kUnknownLanguageCode &&
      !TranslateDownloadManager::IsSupportedLanguage(source_lang)) {
    error_type = TranslateErrors::UNSUPPORTED_LANGUAGE;
  }

  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_AFTER_TRANSLATE,
                                     source_lang, target_lang, error_type,
                                     false);
  NotifyTranslateError(error_type);
}

void TranslateManager::OnTranslateScriptFetchComplete(
    const std::string& source_lang,
    const std::string& target_lang,
    bool success,
    const std::string& data) {
  if (!translate_driver_->HasCurrentPage())
    return;

  if (success) {
    // Translate the page.
    TranslateScript* translate_script =
        TranslateDownloadManager::GetInstance()->script();
    DCHECK(translate_script);
    DoTranslatePage(translate_script->data(), source_lang, target_lang);
  } else {
    translate_client_->ShowTranslateUI(
        translate::TRANSLATE_STEP_TRANSLATE_ERROR, source_lang, target_lang,
        TranslateErrors::NETWORK, false);
    NotifyTranslateError(TranslateErrors::NETWORK);
  }
}

// static
std::string TranslateManager::GetTargetLanguage(const TranslatePrefs* prefs) {
  std::string language;

  // Get the override UI language.
  TranslateExperiment::OverrideUiLanguage(prefs->GetCountry(), &language);

  // If there are no override.
  if (language.empty()) {
    // Get the language from ULP.
    language = TranslateManager::GetTargetLanguageFromULP(prefs);
    if (!language.empty())
      return language;

    // Get the browser's user interface language.
    language = TranslateDownloadManager::GetLanguageCode(
        TranslateDownloadManager::GetInstance()->application_locale());
    // Map 'he', 'nb', 'fil' back to 'iw', 'no', 'tl'
    translate::ToTranslateLanguageSynonym(&language);
  }
  if (TranslateDownloadManager::IsSupportedLanguage(language))
    return language;

  // Will translate to the first supported language on the Accepted Language
  // list or not at all if no such candidate exists.
  std::vector<std::string> accept_languages_list;
  prefs->GetLanguageList(&accept_languages_list);
  for (const auto& lang : accept_languages_list) {
    std::string lang_code = TranslateDownloadManager::GetLanguageCode(lang);
    if (TranslateDownloadManager::IsSupportedLanguage(lang_code))
      return lang_code;
  }
  return std::string();
}

// static
std::string TranslateManager::GetTargetLanguageFromULP(
    const TranslatePrefs* prefs) {
  if (!base::FeatureList::IsEnabled(kTranslateLanguageByULP))
    return std::string();
  std::map<std::string, std::string> params;
  variations::GetVariationParamsByFeature(translate::kTranslateLanguageByULP,
                                          &params);
  TranslatePrefs::LanguageAndProbabilityList reading;
  // We only consider ULP if the confidence is greater than the threshold.
  if (prefs->GetReadingFromUserLanguageProfile(&reading) <=
      GetDoubleFromMap(params, kTargetLanguageULPConfidenceThresholdName,
                       kDefaultTargetLanguageULPConfidenceThreshold))
    return std::string();

  if (reading.size() > 0 &&
      reading[0].second >
          GetDoubleFromMap(params, kTargetLanguageULPProbabilityThresholdName,
                           kDefaultTargetLanguageULPProbabilityThreshold))
    return reading[0].first;
  return std::string();
}

// static
std::string TranslateManager::GetAutoTargetLanguage(
    const std::string& original_language,
    TranslatePrefs* translate_prefs) {
  std::string auto_target_lang;
  if (translate_prefs->ShouldAutoTranslate(original_language,
                                           &auto_target_lang)) {
    // We need to confirm that the saved target language is still supported.
    // Also, GetLanguageCode will take care of removing country code if any.
    auto_target_lang =
        TranslateDownloadManager::GetLanguageCode(auto_target_lang);
    if (TranslateDownloadManager::IsSupportedLanguage(auto_target_lang))
      return auto_target_lang;
  }
  return std::string();
}

LanguageState& TranslateManager::GetLanguageState() {
  return language_state_;
}

bool TranslateManager::ignore_missing_key_for_testing_ = false;

// static
void TranslateManager::SetIgnoreMissingKeyForTesting(bool ignore) {
  ignore_missing_key_for_testing_ = ignore;
}

void TranslateManager::InitTranslateEvent(const std::string& src_lang,
                                          const std::string& dst_lang,
                                          const TranslatePrefs& prefs) {
  translate_event_->Clear();
  translate_event_->set_source_language(src_lang);
  translate_event_->set_target_language(dst_lang);
  translate_event_->set_country(prefs.GetCountry());
  translate_event_->set_accept_count(
      prefs.GetTranslationAcceptedCount(src_lang));
  translate_event_->set_decline_count(
      prefs.GetTranslationDeniedCount(src_lang));
  translate_event_->set_ignore_count(
      prefs.GetTranslationIgnoredCount(src_lang));
  translate_event_->set_ranker_response(
      metrics::TranslateEventProto::NOT_QUERIED);
  translate_event_->set_event_type(metrics::TranslateEventProto::UNKNOWN);
  // TODO(rogerm): Populate the language list.
}

void TranslateManager::RecordTranslateEvent(int event_type) {
  translate_ranker_->RecordTranslateEvent(
      event_type, translate_driver_->GetVisibleURL(), translate_event_.get());
  translate_client_->RecordTranslateEvent(*translate_event_.get());
}

bool TranslateManager::ShouldOverrideDecision(int event_type) {
  return translate_ranker_->ShouldOverrideDecision(
      event_type, translate_driver_->GetVisibleURL(), translate_event_.get());
}

bool TranslateManager::ShouldSuppressBubbleUI(
    bool triggered_from_menu,
    const std::string& source_language) {
  // Suppress the UI if the user navigates to a page with
  // the same language as the previous page. In the new UI,
  // continue offering translation after the user navigates
  // to another page.
  language_state_.SetTranslateEnabled(true);
  if (!base::FeatureList::IsEnabled(translate::kTranslateUI2016Q2) &&
      !language_state_.HasLanguageChanged() &&
      !ShouldOverrideDecision(
          metrics::TranslateEventProto::MATCHES_PREVIOUS_LANGUAGE)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::
            INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE);
    return true;
  }

  // Suppress the UI if the user denied translation for this language
  // too often.
  if (!triggered_from_menu &&
      translate_client_->GetTranslatePrefs()->IsTooOftenDenied(
          source_language) &&
      !ShouldOverrideDecision(
          metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_AUTO_BLACKLIST)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_ABORTED_BY_TOO_OFTEN_DENIED);
    return true;
  }

  return false;
}

}  // namespace translate
