// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
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
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"

namespace translate {

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

// Notifies |g_callback_list_| of translate errors.
void NotifyTranslateError(const TranslateErrorDetails& details) {
  if (!g_callback_list_)
    return;

  g_callback_list_->Notify(details);
}

}  // namespace

TranslateManager::~TranslateManager() {}

// static
scoped_ptr<TranslateManager::TranslateErrorCallbackList::Subscription>
TranslateManager::RegisterTranslateErrorCallback(
    const TranslateManager::TranslateErrorCallback& callback) {
  if (!g_callback_list_)
    g_callback_list_ = new TranslateErrorCallbackList;
  return g_callback_list_->Add(callback);
}

TranslateManager::TranslateManager(
    TranslateClient* translate_client,
    const std::string& accept_languages_pref_name)
    : page_seq_no_(0),
      accept_languages_pref_name_(accept_languages_pref_name),
      translate_client_(translate_client),
      translate_driver_(translate_client_->GetTranslateDriver()),
      language_state_(translate_driver_),
      weak_method_factory_(this) {
}

base::WeakPtr<TranslateManager> TranslateManager::GetWeakPtr() {
  return weak_method_factory_.GetWeakPtr();
}

void TranslateManager::InitiateTranslation(const std::string& page_lang) {
  // Short-circuit out if not in a state where initiating translation makes
  // sense (this method may be called muhtiple times for a given page).
  if (!language_state_.page_needs_translation() ||
      language_state_.translation_pending() ||
      language_state_.translation_declined() ||
      language_state_.IsPageTranslated()) {
    return;
  }

  if (!ignore_missing_key_for_testing_ &&
      !::google_apis::HasKeysConfigured()) {
    // Without an API key, translate won't work, so don't offer to translate
    // in the first place. Leave prefs::kEnableTranslate on, though, because
    // that settings syncs and we don't want to turn off translate everywhere
    // else.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_KEY);
    return;
  }

  PrefService* prefs = translate_client_->GetPrefs();
  if (!prefs->GetBoolean(prefs::kEnableTranslate)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS);
    const std::string& locale =
        TranslateDownloadManager::GetInstance()->application_locale();
    TranslateBrowserMetrics::ReportLocalesOnDisabledByPrefs(locale);
    return;
  }

  // Allow disabling of translate from the command line to assist with
  // automated browser testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          translate::switches::kDisableTranslate)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_SWITCH);
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

  scoped_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());

  std::string target_lang = GetTargetLanguage(translate_prefs.get());
  std::string language_code =
      TranslateDownloadManager::GetLanguageCode(page_lang);

  // Don't translate similar languages (ex: en-US to en).
  if (language_code == target_lang) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES);
    return;
  }

  // Nothing to do if either the language Chrome is in or the language of the
  // page is not supported by the translation server.
  if (target_lang.empty() ||
      !TranslateDownloadManager::IsSupportedLanguage(language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED);
    TranslateBrowserMetrics::ReportUnsupportedLanguageAtInitiation(
        language_code);
    return;
  }

  TranslateAcceptLanguages* accept_languages =
      translate_client_->GetTranslateAcceptLanguages();
  // Don't translate any user black-listed languages.
  if (!translate_prefs->CanTranslateLanguage(accept_languages,
                                             language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
    return;
  }

  // Don't translate any user black-listed URLs.
  if (translate_prefs->IsSiteBlacklisted(page_url.HostNoBrackets())) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
    return;
  }

  // If the user has previously selected "always translate" for this language we
  // automatically translate.  Note that in incognito mode we disable that
  // feature; the user will get an infobar, so they can control whether the
  // page's text is sent to the translate server.
  if (!translate_driver_->IsOffTheRecord()) {
    scoped_ptr<TranslatePrefs> translate_prefs =
        translate_client_->GetTranslatePrefs();
    std::string auto_target_lang =
        GetAutoTargetLanguage(language_code, translate_prefs.get());
    if (!auto_target_lang.empty()) {
      TranslateBrowserMetrics::ReportInitiationStatus(
          TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_CONFIG);
      TranslatePage(language_code, auto_target_lang, false);
      return;
    }
  }

  std::string auto_translate_to = language_state_.AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_LINK);
    TranslatePage(language_code, auto_translate_to, false);
    return;
  }

  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_SHOW_INFOBAR);

  // Prompts the user if they want the page translated.
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
                                     language_code,
                                     target_lang,
                                     TranslateErrors::NONE,
                                     false);
}

void TranslateManager::TranslatePage(const std::string& original_source_lang,
                                     const std::string& target_lang,
                                     bool triggered_from_menu) {
  if (!translate_driver_->HasCurrentPage()) {
    NOTREACHED();
    return;
  }

  // Translation can be kicked by context menu against unsupported languages.
  // Unsupported language strings should be replaced with
  // kUnknownLanguageCode in order to send a translation request with enabling
  // server side auto language detection.
  std::string source_lang(original_source_lang);
  if (!TranslateDownloadManager::IsSupportedLanguage(source_lang))
    source_lang = std::string(translate::kUnknownLanguageCode);

  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING,
                                     source_lang,
                                     target_lang,
                                     TranslateErrors::NONE,
                                     triggered_from_menu);

  TranslateScript* script = TranslateDownloadManager::GetInstance()->script();
  DCHECK(script != NULL);

  const std::string& script_data = script->data();
  if (!script_data.empty()) {
    DoTranslatePage(script_data, source_lang, target_lang);
    return;
  }

  // The script is not available yet.  Queue that request and query for the
  // script.  Once it is downloaded we'll do the translate.
  TranslateScript::RequestCallback callback = base::Bind(
      &TranslateManager::OnTranslateScriptFetchComplete, GetWeakPtr(),
      source_lang, target_lang);

  script->Request(callback);
}

void TranslateManager::RevertTranslation() {
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
      net::AppendQueryParameter(report_error_url,
                                kSourceLanguageQueryName,
                                language_state_.original_language());

  report_error_url = translate::AddHostLocaleToUrl(report_error_url);
  report_error_url = translate::AddApiKeyToUrl(report_error_url);

  translate_client_->ShowReportLanguageDetectionErrorUI(report_error_url);
}

void TranslateManager::DoTranslatePage(const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  language_state_.set_translation_pending(true);
  translate_driver_->TranslatePage(
      page_seq_no_, translate_script, source_lang, target_lang);
}

void TranslateManager::PageTranslated(const std::string& source_lang,
                                      const std::string& target_lang,
                                      TranslateErrors::Type error_type) {
  language_state_.SetCurrentLanguage(target_lang);
  language_state_.set_translation_pending(false);

  if ((error_type == TranslateErrors::NONE) &&
      source_lang != translate::kUnknownLanguageCode &&
      !TranslateDownloadManager::IsSupportedLanguage(source_lang)) {
    error_type = TranslateErrors::UNSUPPORTED_LANGUAGE;
  }

  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_AFTER_TRANSLATE,
                                     source_lang,
                                     target_lang,
                                     error_type,
                                     false);

  if (error_type != TranslateErrors::NONE &&
      !translate_driver_->IsOffTheRecord()) {
    TranslateErrorDetails error_details;
    error_details.time = base::Time::Now();
    error_details.url = translate_driver_->GetLastCommittedURL();
    error_details.error = error_type;
    NotifyTranslateError(error_details);
  }
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
        translate::TRANSLATE_STEP_TRANSLATE_ERROR,
        source_lang,
        target_lang,
        TranslateErrors::NETWORK,
        false);
    if (!translate_driver_->IsOffTheRecord()) {
      TranslateErrorDetails error_details;
      error_details.time = base::Time::Now();
      error_details.url = translate_driver_->GetLastCommittedURL();
      error_details.error = TranslateErrors::NETWORK;
      NotifyTranslateError(error_details);
    }
  }
}

// static
std::string TranslateManager::GetTargetLanguage(const TranslatePrefs* prefs) {
  std::string ui_lang = TranslateDownloadManager::GetLanguageCode(
      TranslateDownloadManager::GetInstance()->application_locale());
  translate::ToTranslateLanguageSynonym(&ui_lang);

  TranslateExperiment::OverrideUiLanguage(prefs->GetCountry(), &ui_lang);

  if (TranslateDownloadManager::IsSupportedLanguage(ui_lang))
    return ui_lang;

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

}  // namespace translate
