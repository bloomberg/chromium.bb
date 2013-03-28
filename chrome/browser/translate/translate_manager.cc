// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/translate/page_translated_details.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/url_constants.h"
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
#include "google_apis/google_api_keys.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/resource/resource_bundle.h"

#ifdef FILE_MANAGER_EXTENSION
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

// The default list of languages the Google translation server supports.
// We use this list until we receive the list that the server exposes.
// For information, here is the list of languages that Chrome can be run in
// but that the translation server does not support:
// am Amharic
// bn Bengali
// gu Gujarati
// kn Kannada
// ml Malayalam
// mr Marathi
// ta Tamil
// te Telugu
const char* const kDefaultSupportedLanguages[] = {
    "af",     // Afrikaans
    "sq",     // Albanian
    "ar",     // Arabic
    "be",     // Belarusian
    "bg",     // Bulgarian
    "ca",     // Catalan
    "zh-CN",  // Chinese (Simplified)
    "zh-TW",  // Chinese (Traditional)
    "hr",     // Croatian
    "cs",     // Czech
    "da",     // Danish
    "nl",     // Dutch
    "en",     // English
    "eo",     // Esperanto
    "et",     // Estonian
    "tl",     // Filipino
    "fi",     // Finnish
    "fr",     // French
    "gl",     // Galician
    "de",     // German
    "el",     // Greek
    "ht",     // Haitian Creole
    "iw",     // Hebrew
    "hi",     // Hindi
    "hu",     // Hungarian
    "is",     // Icelandic
    "id",     // Indonesian
    "ga",     // Irish
    "it",     // Italian
    "ja",     // Japanese
    "ko",     // Korean
    "lv",     // Latvian
    "lt",     // Lithuanian
    "mk",     // Macedonian
    "ms",     // Malay
    "mt",     // Maltese
    "no",     // Norwegian
    "fa",     // Persian
    "pl",     // Polish
    "pt",     // Portuguese
    "ro",     // Romanian
    "ru",     // Russian
    "sr",     // Serbian
    "sk",     // Slovak
    "sl",     // Slovenian
    "es",     // Spanish
    "sw",     // Swahili
    "sv",     // Swedish
    "th",     // Thai
    "tr",     // Turkish
    "uk",     // Ukrainian
    "vi",     // Vietnamese
    "cy",     // Welsh
    "yi",     // Yiddish
};

const char* const kTranslateScriptURL =
    "https://translate.google.com/translate_a/element.js";
const char* const kTranslateScriptQuery =
    "?cb=cr.googleTranslate.onTranslateElementLoad&hl=%s";
const char* const kTranslateScriptHeader =
    "Google-Translate-Element-Mode: library";
const char* const kReportLanguageDetectionErrorURL =
    "https://translate.google.com/translate_error";
const char* const kLanguageListFetchURL =
    "https://translate.googleapis.com/translate_a/l?client=chrome&cb=sl&hl=%s";
const int kMaxRetryLanguageListFetch = 5;
const int kTranslateScriptExpirationDelayDays = 1;

void AddApiKeyToUrl(GURL* url) {
  std::string api_key = google_apis::GetAPIKey();
  std::string query(url->query());
  if (!query.empty())
    query += "&";
  query += "key=" + net::EscapeQueryParamValue(api_key, true);
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  *url = url->ReplaceComponents(replacements);
}

}  // namespace

// This must be kept in sync with the &cb= value in the kLanguageListFetchURL.
const char* const TranslateManager::kLanguageListCallbackName = "sl(";
const char* const TranslateManager::kTargetLanguagesKey = "tl";

// static
base::LazyInstance<std::set<std::string> >
    TranslateManager::supported_languages_ = LAZY_INSTANCE_INITIALIZER;

TranslateManager::~TranslateManager() {
  weak_method_factory_.InvalidateWeakPtrs();
}

// static
TranslateManager* TranslateManager::GetInstance() {
  return Singleton<TranslateManager>::get();
}

// static
bool TranslateManager::IsTranslatableURL(const GURL& url) {
  // A URLs is translatable unless it is one of the following:
  // - an internal URL (chrome:// and others)
  // - the devtools (which is considered UI)
  // - an FTP page (as FTP pages tend to have long lists of filenames that may
  //   confuse the CLD)
  return !url.SchemeIs(chrome::kChromeUIScheme) &&
         !url.SchemeIs(chrome::kChromeDevToolsScheme) &&
         !url.SchemeIs(chrome::kFtpScheme);
}

// static
void TranslateManager::SetSupportedLanguages(const std::string& language_list) {
  // The format is:
  // sl({'sl': {'XX': 'LanguageName', ...}, 'tl': {'XX': 'LanguageName', ...}})
  // Where "sl(" is set in kLanguageListCallbackName
  // and 'tl' is kTargetLanguagesKey
  if (!StartsWithASCII(language_list, kLanguageListCallbackName, false) ||
      !EndsWith(language_list, ")", false)) {
    // We don't have a NOTREACHED here since this can happen in ui_tests, even
    // though the the BrowserMain function won't call us with parameters.ui_task
    // is NULL some tests don't set it, so we must bail here.
    return;
  }
  static const size_t kLanguageListCallbackNameLength =
      strlen(kLanguageListCallbackName);
  std::string languages_json = language_list.substr(
      kLanguageListCallbackNameLength,
      language_list.size() - kLanguageListCallbackNameLength - 1);
  // JSON doesn't support single quotes though this is what is used on the
  // translate server so we must replace them with double quotes.
  ReplaceSubstringsAfterOffset(&languages_json, 0, "'", "\"");
  scoped_ptr<Value> json_value(
      base::JSONReader::Read(languages_json, base::JSON_ALLOW_TRAILING_COMMAS));
  if (json_value == NULL || !json_value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    return;
  }
  // The first level dictionary contains two sub-dict, one for source languages
  // and the other for target languages, we want to use the target languages.
  DictionaryValue* language_dict =
      static_cast<DictionaryValue*>(json_value.get());
  DictionaryValue* target_languages = NULL;
  if (!language_dict->GetDictionary(kTargetLanguagesKey, &target_languages) ||
      target_languages == NULL) {
    NOTREACHED();
    return;
  }
  // Now we can clear our current state...
  std::set<std::string>* supported_languages = supported_languages_.Pointer();
  supported_languages->clear();
  // ... and replace it with the values we just fetched from the server.
  for (DictionaryValue::Iterator iter(*target_languages); !iter.IsAtEnd();
       iter.Advance()) {
    supported_languages_.Pointer()->insert(iter.key());
  }
}

// static
void TranslateManager::InitSupportedLanguages() {
  // If our list of supported languages have not been set yet, we default
  // to our hard coded list of languages in kDefaultSupportedLanguages.
  if (supported_languages_.Pointer()->empty()) {
    for (size_t i = 0; i < arraysize(kDefaultSupportedLanguages); ++i)
      supported_languages_.Pointer()->insert(kDefaultSupportedLanguages[i]);
  }
}

// static
void TranslateManager::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  InitSupportedLanguages();
  std::set<std::string>* supported_languages = supported_languages_.Pointer();
  std::set<std::string>::const_iterator iter = supported_languages->begin();
  for (; iter != supported_languages->end(); ++iter)
    languages->push_back(*iter);
}

// static
std::string TranslateManager::GetLanguageCode(
    const std::string& chrome_locale) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(chrome_locale))
    return chrome_locale;

  size_t hypen_index = chrome_locale.find('-');
  if (hypen_index == std::string::npos)
    return chrome_locale;
  return chrome_locale.substr(0, hypen_index);
}

// static
bool TranslateManager::IsSupportedLanguage(const std::string& page_language) {
  InitSupportedLanguages();
  return supported_languages_.Pointer()->count(page_language) != 0;
}

void TranslateManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          content::Source<NavigationController>(source).ptr();
      content::LoadCommittedDetails* load_details =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      NavigationEntry* entry = controller->GetActiveEntry();
      if (!entry) {
        NOTREACHED();
        return;
      }

      TranslateTabHelper* translate_tab_helper =
          TranslateTabHelper::FromWebContents(controller->GetWebContents());
      if (!translate_tab_helper)
        return;

      // If the navigation happened while offline don't show the translate
      // bar since there will be nothing to translate.
      if (load_details->http_status_code == 0 ||
          load_details->http_status_code == 500) {
        return;
      }

      if (!load_details->is_main_frame &&
          translate_tab_helper->language_state().translation_declined()) {
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
      // page is translatable.
      if (!translate_tab_helper->language_state().page_translatable())
        return;
      // Note that we delay it as the TranslateManager gets this notification
      // before the WebContents and the WebContents processing might remove the
      // current infobars.  Since InitTranslation might add an infobar, it must
      // be done after that.
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(
              &TranslateManager::InitiateTranslationPosted,
              weak_method_factory_.GetWeakPtr(),
              controller->GetWebContents()->GetRenderProcessHost()->GetID(),
              controller->GetWebContents()->GetRenderViewHost()->GetRoutingID(),
              translate_tab_helper->language_state().original_language()));
      break;
    }
    case chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED: {
      WebContents* tab = content::Source<WebContents>(source).ptr();
      // We may get this notifications multiple times.  Make sure to translate
      // only once.
      TranslateTabHelper* translate_tab_helper =
          TranslateTabHelper::FromWebContents(tab);
      if (!translate_tab_helper)
        return;

      LanguageState& language_state = translate_tab_helper->language_state();
      if (language_state.page_translatable() &&
          !language_state.translation_pending() &&
          !language_state.translation_declined() &&
          !language_state.IsPageTranslated()) {
        std::string language = *(content::Details<std::string>(details).ptr());
        InitiateTranslation(tab, language);
      }
      break;
    }
    case chrome::NOTIFICATION_PAGE_TRANSLATED: {
      // Only add translate infobar if it doesn't exist; if it already exists,
      // just update the state, the actual infobar would have received the same
      //  notification and update the visual display accordingly.
      WebContents* tab = content::Source<WebContents>(source).ptr();
      PageTranslatedDetails* page_translated_details =
          content::Details<PageTranslatedDetails>(details).ptr();
      PageTranslated(tab, page_translated_details);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      PrefService* pref_service =
          content::Source<Profile>(source).ptr()->GetPrefs();
      notification_registrar_.Remove(this,
                                     chrome::NOTIFICATION_PROFILE_DESTROYED,
                                     source);
      size_t count = accept_languages_.erase(pref_service);
      // We should know about this profile since we are listening for
      // notifications on it.
      DCHECK(count == 1u);
      PrefChangeRegistrar* pref_change_registrar =
          pref_change_registrars_[pref_service];
      count = pref_change_registrars_.erase(pref_service);
      DCHECK(count == 1u);
      delete pref_change_registrar;
      break;
    }
    default:
      NOTREACHED();
  }
}

void TranslateManager::OnURLFetchComplete(const net::URLFetcher* source) {
  if (translate_script_request_pending_.get() != source &&
      language_list_request_pending_.get() != source) {
    // Looks like crash on Mac is possibly caused with callback entering here
    // with unknown fetcher when network is refreshed.
    scoped_ptr<const net::URLFetcher> delete_ptr(source);
    return;
  }

  bool error =
      (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      source->GetResponseCode() != 200);
  if (translate_script_request_pending_.get() == source) {
    scoped_ptr<const net::URLFetcher> delete_ptr(
        translate_script_request_pending_.release());
    if (!error) {
      base::StringPiece str = ResourceBundle::GetSharedInstance().
          GetRawDataResource(IDR_TRANSLATE_JS);
      DCHECK(translate_script_.empty());
      str.CopyToString(&translate_script_);
      std::string argument = "('";
      std::string api_key = google_apis::GetAPIKey();
      argument += net::EscapeQueryParamValue(api_key, true);
      argument += "');\n";
      std::string data;
      source->GetResponseAsString(&data);
      translate_script_ += argument + data;
      // We'll expire the cached script after some time, to make sure long
      // running browsers still get fixes that might get pushed with newer
      // scripts.
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          base::Bind(&TranslateManager::ClearTranslateScript,
                     weak_method_factory_.GetWeakPtr()),
          translate_script_expiration_delay_);
    }
    // Process any pending requests.
    std::vector<PendingRequest>::const_iterator iter;
    for (iter = pending_requests_.begin(); iter != pending_requests_.end();
         ++iter) {
      const PendingRequest& request = *iter;
      WebContents* web_contents =
          tab_util::GetWebContentsByID(request.render_process_id,
                                       request.render_view_id);
      if (!web_contents) {
        // The tab went away while we were retrieving the script.
        continue;
      }
      NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
      if (!entry || entry->GetPageID() != request.page_id) {
        // We navigated away from the page the translation was triggered on.
        continue;
      }

      if (error) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        TranslateInfoBarDelegate::Create(
            InfoBarService::FromWebContents(web_contents),
            true,
            TranslateInfoBarDelegate::TRANSLATION_ERROR,
            TranslateErrors::NETWORK,
            profile->GetPrefs(),
            request.source_lang,
            request.target_lang);
      } else {
        // Translate the page.
        DoTranslatePage(web_contents, translate_script_,
                        request.source_lang, request.target_lang);
      }
    }
    pending_requests_.clear();
  } else {  // if (translate_script_request_pending_.get() == source)
    scoped_ptr<const net::URLFetcher> delete_ptr(
        language_list_request_pending_.release());
    if (!error) {
      std::string data;
      source->GetResponseAsString(&data);
      SetSupportedLanguages(data);
    } else {
      VLOG(9) << "Failed to Fetch languages from: " << kLanguageListFetchURL;
    }
  }
}

TranslateManager::TranslateManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_method_factory_(this)),
      translate_script_expiration_delay_(
          base::TimeDelta::FromDays(kTranslateScriptExpirationDelayDays)) {
  notification_registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_PAGE_TRANSLATED,
                              content::NotificationService::AllSources());
}

void TranslateManager::InitiateTranslation(WebContents* web_contents,
                                           const std::string& page_lang) {
#ifdef FILE_MANAGER_EXTENSION
  const GURL& page_url = web_contents->GetURL();
  if (page_url.SchemeIs("chrome-extension") &&
      page_url.DomainIs(kFileBrowserDomain))
    return;
#endif

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetOriginalProfile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kEnableTranslate))
    return;

  // Allow disabling of translate from the command line to assist with
  // automated browser testing.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableTranslate))
    return;

  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  if (!entry) {
    // This can happen for popups created with window.open("").
    return;
  }

  std::string target_lang = GetTargetLanguage(prefs);
  std::string language_code = GetLanguageCode(page_lang);
  // Nothing to do if either the language Chrome is in or the language of the
  // page is not supported by the translation server.
  if (target_lang.empty() || !IsSupportedLanguage(language_code)) {
    return;
  }

  // We don't want to translate:
  // - any Chrome specific page (New Tab Page, Download, History... pages).
  // - similar languages (ex: en-US to en).
  // - any user black-listed URLs or user selected language combination.
  // - any language the user configured as accepted languages.
  if (!IsTranslatableURL(entry->GetURL()) || language_code == target_lang ||
      !TranslatePrefs::CanTranslate(prefs, language_code, entry->GetURL()) ||
      IsAcceptLanguage(web_contents, language_code)) {
    return;
  }

  // If the user has previously selected "always translate" for this language we
  // automatically translate.  Note that in incognito mode we disable that
  // feature; the user will get an infobar, so they can control whether the
  // page's text is sent to the translate server.
  std::string auto_target_lang;
  if (!web_contents->GetBrowserContext()->IsOffTheRecord() &&
      TranslatePrefs::ShouldAutoTranslate(prefs, language_code,
          &auto_target_lang)) {
    // We need to confirm that the saved target language is still supported.
    // Also, GetLanguageCode will take care of removing country code if any.
    auto_target_lang = GetLanguageCode(auto_target_lang);
    if (IsSupportedLanguage(auto_target_lang)) {
      TranslatePage(web_contents, language_code, auto_target_lang);
      return;
    }
  }

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  if (!translate_tab_helper)
    return;

  std::string auto_translate_to =
      translate_tab_helper->language_state().AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslatePage(web_contents, language_code, auto_translate_to);
    return;
  }

  // Prompts the user if he/she wants the page translated.
  TranslateInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), false,
      TranslateInfoBarDelegate::BEFORE_TRANSLATE, TranslateErrors::NONE,
      profile->GetPrefs(), language_code, target_lang);
}

void TranslateManager::InitiateTranslationPosted(
    int process_id, int render_id, const std::string& page_lang) {
  // The tab might have been closed.
  WebContents* web_contents =
      tab_util::GetWebContentsByID(process_id, render_id);
  if (!web_contents)
    return;

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  if (translate_tab_helper->language_state().translation_pending())
    return;

  InitiateTranslation(web_contents, GetLanguageCode(page_lang));
}

void TranslateManager::TranslatePage(WebContents* web_contents,
                                     const std::string& source_lang,
                                     const std::string& target_lang) {
  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  TranslateInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), true,
      TranslateInfoBarDelegate::TRANSLATING, TranslateErrors::NONE,
      profile->GetPrefs(), source_lang, target_lang);

  if (!translate_script_.empty()) {
    DoTranslatePage(web_contents, translate_script_, source_lang, target_lang);
    return;
  }

  // The script is not available yet.  Queue that request and query for the
  // script.  Once it is downloaded we'll do the translate.
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  PendingRequest request;
  request.render_process_id = rvh->GetProcess()->GetID();
  request.render_view_id = rvh->GetRoutingID();
  request.page_id = entry->GetPageID();
  request.source_lang = source_lang;
  request.target_lang = target_lang;
  pending_requests_.push_back(request);
  RequestTranslateScript();
}

void TranslateManager::RevertTranslation(WebContents* web_contents) {
  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }
  web_contents->GetRenderViewHost()->Send(new ChromeViewMsg_RevertTranslation(
      web_contents->GetRenderViewHost()->GetRoutingID(), entry->GetPageID()));

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  translate_tab_helper->language_state().set_current_language(
      translate_tab_helper->language_state().original_language());
}

void TranslateManager::ReportLanguageDetectionError(WebContents* web_contents) {
  UMA_HISTOGRAM_COUNTS("Translate.ReportLanguageDetectionError", 1);
  // We'll open the URL in a new tab so that the user can tell us more.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    NOTREACHED();
    return;
  }

  GURL page_url = web_contents->GetController().GetActiveEntry()->GetURL();
  // Report option should be disabled for secure URLs.
  DCHECK(!page_url.SchemeIsSecure());
  std::string report_error_url_str(kReportLanguageDetectionErrorURL);
  report_error_url_str += "?client=cr&action=langidc&u=";
  report_error_url_str += net::EscapeUrlEncodedData(page_url.spec(), true);
  report_error_url_str += "&sl=";

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  report_error_url_str +=
      translate_tab_helper->language_state().original_language();
  report_error_url_str += "&hl=";
  report_error_url_str +=
      GetLanguageCode(g_browser_process->GetApplicationLocale());

  GURL report_error_url(report_error_url_str);
  AddApiKeyToUrl(&report_error_url);
  chrome::AddSelectedTabWithURL(browser, report_error_url,
                                content::PAGE_TRANSITION_AUTO_BOOKMARK);
}

void TranslateManager::DoTranslatePage(WebContents* web_contents,
                                       const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  TranslateTabHelper* translate_tab_helper =
      TranslateTabHelper::FromWebContents(web_contents);
  if (!translate_tab_helper)
    return;

  translate_tab_helper->language_state().set_translation_pending(true);
  web_contents->GetRenderViewHost()->Send(new ChromeViewMsg_TranslatePage(
      web_contents->GetRenderViewHost()->GetRoutingID(), entry->GetPageID(),
      translate_script, source_lang, target_lang));
}

void TranslateManager::PageTranslated(WebContents* web_contents,
                                      PageTranslatedDetails* details) {
  if ((details->error_type == TranslateErrors::NONE) &&
      !IsSupportedLanguage(details->source_language)) {
    // TODO(jcivelli): http://crbug.com/9390 We should change the "after
    //                 translate" infobar to support unknown as the original
    //                 language.
    UMA_HISTOGRAM_COUNTS("Translate.ServerReportedUnsupportedLanguage", 1);
    details->error_type = TranslateErrors::UNSUPPORTED_LANGUAGE;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  TranslateInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), true,
      (details->error_type == TranslateErrors::NONE) ?
          TranslateInfoBarDelegate::AFTER_TRANSLATE :
          TranslateInfoBarDelegate::TRANSLATION_ERROR,
      details->error_type, prefs, details->source_language,
      details->target_language);
}

bool TranslateManager::IsAcceptLanguage(WebContents* web_contents,
                                        const std::string& language) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  profile = profile->GetOriginalProfile();
  PrefService* pref_service = profile->GetPrefs();
  PrefServiceLanguagesMap::const_iterator iter =
      accept_languages_.find(pref_service);
  if (iter == accept_languages_.end()) {
    InitAcceptLanguages(pref_service);
    // Listen for this profile going away, in which case we would need to clear
    // the accepted languages for the profile.
    notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                                content::Source<Profile>(profile));
    // Also start listening for changes in the accept languages.
    DCHECK(pref_change_registrars_.find(pref_service) ==
           pref_change_registrars_.end());
    PrefChangeRegistrar* pref_change_registrar = new PrefChangeRegistrar;
    pref_change_registrar->Init(pref_service);
    pref_change_registrar->Add(
        prefs::kAcceptLanguages,
        base::Bind(&TranslateManager::InitAcceptLanguages,
                   base::Unretained(this),
                   pref_service));
    pref_change_registrars_[pref_service] = pref_change_registrar;

    iter = accept_languages_.find(pref_service);
  }

  return iter->second.count(language) != 0;
}

void TranslateManager::InitAcceptLanguages(PrefService* prefs) {
  // We have been asked for this profile, build the languages.
  std::string accept_langs_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_langs_list;
  LanguageSet accept_langs_set;
  base::SplitString(accept_langs_str, ',', &accept_langs_list);
  std::vector<std::string>::const_iterator iter;
  std::string ui_lang =
      GetLanguageCode(g_browser_process->GetApplicationLocale());
  bool is_ui_english = StartsWithASCII(ui_lang, "en-", false);
  for (iter = accept_langs_list.begin();
       iter != accept_langs_list.end(); ++iter) {
    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    std::string accept_lang(*iter);
    size_t index = iter->find("-");
    if (index != std::string::npos && *iter != "zh-CN" && *iter != "zh-TW")
      accept_lang = iter->substr(0, index);
    // Special-case English until we resolve bug 36182 properly.
    // Add English only if the UI language is not English. This will annoy
    // users of non-English Chrome who can comprehend English until English is
    // black-listed.
    // TODO(jungshik): Once we determine that it's safe to remove English from
    // the default Accept-Language values for most locales, remove this
    // special-casing.
    if (accept_lang != "en" || is_ui_english)
      accept_langs_set.insert(accept_lang);
  }
  accept_languages_[prefs] = accept_langs_set;
}

void TranslateManager::FetchLanguageListFromTranslateServer(
    PrefService* prefs) {
  if (language_list_request_pending_.get() != NULL)
    return;

  // We don't want to do this when translate is disabled.
  DCHECK(prefs != NULL);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTranslate) ||
      (prefs != NULL && !prefs->GetBoolean(prefs::kEnableTranslate))) {
    return;
  }

  GURL language_list_fetch_url = GURL(
      base::StringPrintf(
          kLanguageListFetchURL,
          GetLanguageCode(g_browser_process->GetApplicationLocale()).c_str()));
  AddApiKeyToUrl(&language_list_fetch_url);
  language_list_request_pending_.reset(net::URLFetcher::Create(
      1, language_list_fetch_url, net::URLFetcher::GET, this));
  language_list_request_pending_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                               net::LOAD_DO_NOT_SAVE_COOKIES);
  language_list_request_pending_->SetRequestContext(
      g_browser_process->system_request_context());
  language_list_request_pending_->SetMaxRetriesOn5xx(
      kMaxRetryLanguageListFetch);
  language_list_request_pending_->Start();
}

void TranslateManager::CleanupPendingUlrFetcher() {
  language_list_request_pending_.reset();
  translate_script_request_pending_.reset();
}

void TranslateManager::RequestTranslateScript() {
  if (translate_script_request_pending_.get() != NULL)
    return;

  GURL translate_script_url;
  std::string translate_script;
  // Check if command-line contains an alternative URL for translate service.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTranslateScriptURL)) {
    translate_script = std::string(
        command_line.GetSwitchValueASCII(switches::kTranslateScriptURL));
    translate_script_url = GURL(translate_script.c_str());
    if (!translate_script_url.is_valid() ||
        !translate_script_url.query().empty()) {
      LOG(WARNING) << "The following translate URL specified at the "
                   << "command-line is invalid: " << translate_script;
      translate_script.clear();
    }
  }
  // Use default URL when command-line argument is not specified, or specified
  // URL is invalid.
  if (translate_script.empty())
    translate_script = std::string(kTranslateScriptURL);

  translate_script += std::string(kTranslateScriptQuery);
  translate_script_url = GURL(base::StringPrintf(
      translate_script.c_str(),
      GetLanguageCode(g_browser_process->GetApplicationLocale()).c_str()));
  AddApiKeyToUrl(&translate_script_url);
  translate_script_request_pending_.reset(net::URLFetcher::Create(
      0, translate_script_url, net::URLFetcher::GET, this));
  translate_script_request_pending_->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  translate_script_request_pending_->SetRequestContext(
      g_browser_process->system_request_context());
  translate_script_request_pending_->SetExtraRequestHeaders(
      kTranslateScriptHeader);
  translate_script_request_pending_->Start();
}

// static
std::string TranslateManager::GetTargetLanguage(PrefService* prefs) {
  std::string ui_lang =
    GetLanguageCode(g_browser_process->GetApplicationLocale());
  if (IsSupportedLanguage(ui_lang))
    return ui_lang;

  // Getting the accepted languages list
  std::string accept_langs_str = prefs->GetString(prefs::kAcceptLanguages);

  std::vector<std::string> accept_langs_list;
  base::SplitString(accept_langs_str, ',', &accept_langs_list);

  // Will translate to the first supported language on the Accepted Language
  // list or not at all if no such candidate exists
  std::vector<std::string>::iterator iter;
  for (iter = accept_langs_list.begin();
       iter != accept_langs_list.end(); ++iter) {
    std::string lang_code = GetLanguageCode(*iter);
    if (IsSupportedLanguage(lang_code))
      return lang_code;
  }
  return std::string();
}
