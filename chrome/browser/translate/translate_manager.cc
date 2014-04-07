// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "extensions/common/constants.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

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

// The maximum number of attempts we'll do to see if the page has finshed
// loading before giving up the translation
const int kMaxTranslateLoadCheckAttempts = 20;

// Notifies |g_callback_list_| of translate errors.
void NotifyTranslateError(const TranslateErrorDetails& details) {
  if (!g_callback_list_)
    return;

  g_callback_list_->Notify(details);
}

}  // namespace

TranslateManager::~TranslateManager() {}

// static
bool TranslateManager::IsTranslatableURL(const GURL& url) {
  // A URLs is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL (chrome:// and others)
  // - the devtools (which is considered UI)
  // - Chrome OS file manager extension
  // - an FTP page (as FTP pages tend to have long lists of filenames that may
  //   confuse the CLD)
  return !url.is_empty() &&
         !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) &&
#if defined(OS_CHROMEOS)
         !(url.SchemeIs(extensions::kExtensionScheme) &&
           url.DomainIs(file_manager::kFileManagerAppId)) &&
#endif
         !url.SchemeIs(content::kFtpScheme);
}

void TranslateManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          content::Source<NavigationController>(source).ptr();
      DCHECK_EQ(&translate_tab_helper_->GetWebContents()->GetController(),
                controller);
      content::LoadCommittedDetails* load_details =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      NavigationEntry* entry = controller->GetActiveEntry();
      if (!entry) {
        NOTREACHED();
        return;
      }

      if (!translate_tab_helper_->GetWebContents())
        return;

      // If the navigation happened while offline don't show the translate
      // bar since there will be nothing to translate.
      if (load_details->http_status_code == 0 ||
          load_details->http_status_code == net::HTTP_INTERNAL_SERVER_ERROR) {
        return;
      }

      if (!load_details->is_main_frame &&
          translate_driver_->GetLanguageState().translation_declined()) {
        // Some sites (such as Google map) may trigger sub-frame navigations
        // when the user interacts with the page.  We don't want to show a new
        // infobar if the user already dismissed one in that case.
        return;
      }
      if (entry->GetTransitionType() != content::PAGE_TRANSITION_RELOAD &&
          load_details->type != content::NAVIGATION_TYPE_SAME_PAGE) {
        return;
      }

      // When doing a page reload, TAB_LANGUAGE_DETERMINED is not sent,
      // so the translation needs to be explicitly initiated, but only when the
      // page needs translation.
      if (!translate_driver_->GetLanguageState().page_needs_translation())
        return;
      // Note that we delay it as the TranslateManager gets this notification
      // before the WebContents and the WebContents processing might remove the
      // current infobars.  Since InitTranslation might add an infobar, it must
      // be done after that.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &TranslateManager::InitiateTranslationPosted,
              weak_method_factory_.GetWeakPtr(),
              translate_driver_->GetLanguageState().original_language(),
              0));
      break;
    }
    case chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED: {
      const LanguageDetectionDetails* lang_det_details =
          content::Details<const LanguageDetectionDetails>(details).ptr();

      WebContents* tab = content::Source<WebContents>(source).ptr();
      DCHECK_EQ(translate_tab_helper_->GetWebContents(), tab);

      if (!translate_tab_helper_->GetWebContents())
        return;

      // We may get this notifications multiple times.  Make sure to translate
      // only once.
      LanguageState& language_state = translate_driver_->GetLanguageState();
      if (language_state.page_needs_translation() &&
          !language_state.translation_pending() &&
          !language_state.translation_declined() &&
          !language_state.IsPageTranslated()) {
        std::string language = lang_det_details->adopted_language;
        InitiateTranslation(language);
      }
      break;
    }
    case chrome::NOTIFICATION_PAGE_TRANSLATED: {
      // Only add translate infobar if it doesn't exist; if it already exists,
      // just update the state, the actual infobar would have received the same
      //  notification and update the visual display accordingly.
      PageTranslatedDetails* page_translated_details =
          content::Details<PageTranslatedDetails>(details).ptr();
      PageTranslated(page_translated_details);
      break;
    }
    default:
      NOTREACHED();
  }
}

// static
scoped_ptr<TranslateManager::TranslateErrorCallbackList::Subscription>
TranslateManager::RegisterTranslateErrorCallback(
    const TranslateManager::TranslateErrorCallback& callback) {
  if (!g_callback_list_)
    g_callback_list_ = new TranslateErrorCallbackList;
  return g_callback_list_->Add(callback);
}

TranslateManager::TranslateManager(
    TranslateTabHelper* helper,
    const std::string& accept_languages_pref_name)
    : max_reload_check_attempts_(kMaxTranslateLoadCheckAttempts),
      accept_languages_pref_name_(accept_languages_pref_name),
      translate_tab_helper_(helper),
      translate_client_(helper),
      translate_driver_(translate_client_->GetTranslateDriver()),
      weak_method_factory_(this) {

  WebContents* web_contents = translate_tab_helper_->GetWebContents();

  notification_registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&web_contents->GetController()));
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                              content::Source<WebContents>(web_contents));
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PAGE_TRANSLATED,
                              content::Source<WebContents>(web_contents));
}

void TranslateManager::InitiateTranslation(const std::string& page_lang) {
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          translate::switches::kDisableTranslate)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_SWITCH);
    return;
  }

  // MHTML pages currently cannot be translated.
  // See bug: 217945.
  WebContents* web_contents = translate_tab_helper_->GetWebContents();
  if (web_contents->GetContentsMimeType() == "multipart/related") {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED);
    return;
  }

  // Don't translate any Chrome specific page, e.g., New Tab Page, Download,
  // History, and so on.
  GURL page_url = web_contents->GetURL();
  if (!IsTranslatableURL(page_url)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED);
    return;
  }

  // Get the accepted languages list.
  std::vector<std::string> accept_languages_list;
  base::SplitString(prefs->GetString(accept_languages_pref_name_.c_str()), ',',
                    &accept_languages_list);

  std::string target_lang = GetTargetLanguage(accept_languages_list);
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

  scoped_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());

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
  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    std::string auto_target_lang = GetAutoTargetLanguage(language_code, prefs);
    if (!auto_target_lang.empty()) {
      TranslateBrowserMetrics::ReportInitiationStatus(
          TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_CONFIG);
      TranslatePage(language_code, auto_target_lang, false);
      return;
    }
  }

  LanguageState& language_state = translate_driver_->GetLanguageState();
  std::string auto_translate_to = language_state.AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_LINK);
    TranslatePage(language_code, auto_translate_to, false);
    return;
  }

  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_SHOW_INFOBAR);

  // Prompts the user if he/she wants the page translated.
  translate_tab_helper_->ShowTranslateUI(
      translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
      language_code,
      target_lang,
      TranslateErrors::NONE,
      false);
}

void TranslateManager::InitiateTranslationPosted(const std::string& page_lang,
                                                 int attempt) {
  WebContents* web_contents = translate_tab_helper_->GetWebContents();
  DCHECK(web_contents);

  if (translate_driver_->GetLanguageState().translation_pending())
    return;

  // During a reload we need web content to be available before the
  // translate script is executed. Otherwise we will run the translate script on
  // an empty DOM which will fail. Therefore we wait a bit to see if the page
  // has finished.
  if ((web_contents->IsLoading()) && attempt < kMaxTranslateLoadCheckAttempts) {
    int backoff = attempt * max_reload_check_attempts_;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&TranslateManager::InitiateTranslationPosted,
                              weak_method_factory_.GetWeakPtr(),
                              page_lang, ++attempt),
        base::TimeDelta::FromMilliseconds(backoff));
    return;
  }

  InitiateTranslation(TranslateDownloadManager::GetLanguageCode(page_lang));
}

void TranslateManager::TranslatePage(const std::string& original_source_lang,
                                     const std::string& target_lang,
                                     bool triggered_from_menu) {
  WebContents* web_contents = translate_tab_helper_->GetWebContents();
  DCHECK(web_contents);
  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  if (!entry) {
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

  translate_tab_helper_->ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING,
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
  TranslateScript::RequestCallback callback =
      base::Bind(&TranslateManager::OnTranslateScriptFetchComplete,
                 weak_method_factory_.GetWeakPtr(),
                 entry->GetPageID(),
                 source_lang,
                 target_lang);

  script->Request(callback);
}

void TranslateManager::RevertTranslation() {
  translate_driver_->RevertTranslation();
  translate_driver_->GetLanguageState().SetCurrentLanguage(
      translate_driver_->GetLanguageState().original_language());
}

void TranslateManager::ReportLanguageDetectionError() {
  TranslateBrowserMetrics::ReportLanguageDetectionError();
  // We'll open the URL in a new tab so that the user can tell us more.
  WebContents* web_contents = translate_tab_helper_->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    NOTREACHED();
    return;
  }

  GURL report_error_url = GURL(kReportLanguageDetectionErrorURL);

  GURL page_url = web_contents->GetController().GetActiveEntry()->GetURL();
  report_error_url = net::AppendQueryParameter(
      report_error_url,
      kUrlQueryName,
      page_url.spec());

  report_error_url = net::AppendQueryParameter(
      report_error_url,
      kSourceLanguageQueryName,
      translate_driver_->GetLanguageState().original_language());

  report_error_url = TranslateURLUtil::AddHostLocaleToUrl(report_error_url);
  report_error_url = TranslateURLUtil::AddApiKeyToUrl(report_error_url);

  chrome::AddSelectedTabWithURL(browser, report_error_url,
                                content::PAGE_TRANSITION_AUTO_BOOKMARK);
}

void TranslateManager::DoTranslatePage(const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  translate_driver_->GetLanguageState().set_translation_pending(true);
  translate_driver_->TranslatePage(translate_script, source_lang, target_lang);
}

void TranslateManager::PageTranslated(PageTranslatedDetails* details) {
  if ((details->error_type == TranslateErrors::NONE) &&
      details->source_language != translate::kUnknownLanguageCode &&
      !TranslateDownloadManager::IsSupportedLanguage(
           details->source_language)) {
    details->error_type = TranslateErrors::UNSUPPORTED_LANGUAGE;
  }

  DCHECK(translate_tab_helper_->GetWebContents());

  translate_tab_helper_->ShowTranslateUI(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      details->source_language,
      details->target_language,
      details->error_type,
      false);

  WebContents* web_contents = translate_tab_helper_->GetWebContents();
  if (details->error_type != TranslateErrors::NONE &&
      !web_contents->GetBrowserContext()->IsOffTheRecord()) {
    TranslateErrorDetails error_details;
    error_details.time = base::Time::Now();
    error_details.url = web_contents->GetLastCommittedURL();
    error_details.error = details->error_type;
    NotifyTranslateError(error_details);
  }
}

void TranslateManager::OnTranslateScriptFetchComplete(
    int page_id,
    const std::string& source_lang,
    const std::string& target_lang,
    bool success,
    const std::string& data) {
  WebContents* web_contents = translate_tab_helper_->GetWebContents();
  DCHECK(web_contents);
  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  if (!entry || entry->GetPageID() != page_id) {
    // We navigated away from the page the translation was triggered on.
    return;
  }

  if (success) {
    // Translate the page.
    TranslateScript* translate_script =
        TranslateDownloadManager::GetInstance()->script();
    DCHECK(translate_script);
    DoTranslatePage(translate_script->data(), source_lang, target_lang);
  } else {
    translate_tab_helper_->ShowTranslateUI(
        translate::TRANSLATE_STEP_TRANSLATE_ERROR,
        source_lang,
        target_lang,
        TranslateErrors::NETWORK,
        false);
    if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
      TranslateErrorDetails error_details;
      error_details.time = base::Time::Now();
      error_details.url = entry->GetURL();
      error_details.error = TranslateErrors::NETWORK;
      NotifyTranslateError(error_details);
    }
  }
}

// static
std::string TranslateManager::GetTargetLanguage(
    const std::vector<std::string>& accept_languages_list) {
  std::string ui_lang = TranslatePrefs::ConvertLangCodeForTranslation(
      TranslateDownloadManager::GetLanguageCode(
          TranslateDownloadManager::GetInstance()->application_locale()));

  if (TranslateDownloadManager::IsSupportedLanguage(ui_lang))
    return ui_lang;

  // Will translate to the first supported language on the Accepted Language
  // list or not at all if no such candidate exists
  std::vector<std::string>::const_iterator iter;
  for (iter = accept_languages_list.begin();
       iter != accept_languages_list.end(); ++iter) {
    std::string lang_code = TranslateDownloadManager::GetLanguageCode(*iter);
    if (TranslateDownloadManager::IsSupportedLanguage(lang_code))
      return lang_code;
  }
  return std::string();
}

// static
std::string TranslateManager::GetAutoTargetLanguage(
    const std::string& original_language,
    PrefService* prefs) {
  std::string auto_target_lang;
  scoped_ptr<TranslatePrefs> translate_prefs(
      TranslateTabHelper::CreateTranslatePrefs(prefs));
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
