// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/singleton.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/translate/page_translated_details.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Mapping from a locale name to a language code name.
// Locale names not included are translated as is.
struct LocaleToCLDLanguage {
  const char* locale_language;  // Language Chrome locale is in.
  const char* cld_language;     // Language the CLD reports.
};
LocaleToCLDLanguage kLocaleToCLDLanguages[] = {
    { "en-GB", "en" },
    { "en-US", "en" },
    { "es-419", "es" },
    { "pt-BR", "pt" },
    { "pt-PT", "pt" },
};

// The list of languages the Google translation server supports.
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
const char* kSupportedLanguages[] = {
    "af",     // Afrikaans
    "az",     // Azerbaijani
    "sq",     // Albanian
    "ar",     // Arabic
    "hy",     // Armenian
    "eu",     // Basque
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
    "et",     // Estonian
    "fi",     // Finnish
    "fil",    // Filipino
    "fr",     // French
    "gl",     // Galician
    "de",     // German
    "el",     // Greek
    "ht",     // Haitian Creole
    "he",     // Hebrew
    "hi",     // Hindi
    "hu",     // Hungarian
    "is",     // Icelandic
    "id",     // Indonesian
    "it",     // Italian
    "ga",     // Irish
    "ja",     // Japanese
    "ka",     // Georgian
    "ko",     // Korean
    "lv",     // Latvian
    "lt",     // Lithuanian
    "mk",     // Macedonian
    "ms",     // Malay
    "mt",     // Maltese
    "nb",     // Norwegian
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
    "ur",     // Urdu
    "vi",     // Vietnamese
    "cy",     // Welsh
    "yi",     // Yiddish
};

const char* const kTranslateScriptURL =
    "http://translate.google.com/translate_a/element.js?"
    "cb=cr.googleTranslate.onTranslateElementLoad";
const char* const kTranslateScriptHeader =
    "Google-Translate-Element-Mode: library";
const char* const kReportLanguageDetectionErrorURL =
    "http://translate.google.com/translate_error";

const int kTranslateScriptExpirationDelayMS = 24 * 60 * 60 * 1000;  // 1 day.

}  // namespace

// static
base::LazyInstance<std::set<std::string> >
    TranslateManager::supported_languages_(base::LINKER_INITIALIZED);

TranslateManager::~TranslateManager() {
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
void TranslateManager::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  for (size_t i = 0; i < arraysize(kSupportedLanguages); ++i)
    languages->push_back(kSupportedLanguages[i]);
}

// static
std::string TranslateManager::GetLanguageCode(
    const std::string& chrome_locale) {
  for (size_t i = 0; i < arraysize(kLocaleToCLDLanguages); ++i) {
    if (chrome_locale == kLocaleToCLDLanguages[i].locale_language)
      return kLocaleToCLDLanguages[i].cld_language;
  }
  return chrome_locale;
}

// static
bool TranslateManager::IsSupportedLanguage(const std::string& page_language) {
  if (supported_languages_.Pointer()->empty()) {
    for (size_t i = 0; i < arraysize(kSupportedLanguages); ++i)
      supported_languages_.Pointer()->insert(kSupportedLanguages[i]);
  }
  return supported_languages_.Pointer()->find(page_language) !=
      supported_languages_.Pointer()->end();
}

void TranslateManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      NavigationController::LoadCommittedDetails* load_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      NavigationEntry* entry = controller->GetActiveEntry();
      if (!entry) {
        NOTREACHED();
        return;
      }
      if (!load_details->is_main_frame &&
          controller->tab_contents()->language_state().translation_declined()) {
        // Some sites (such as Google map) may trigger sub-frame navigations
        // when the user interacts with the page.  We don't want to show a new
        // infobar if the user already dismissed one in that case.
        return;
      }
      if (entry->transition_type() != PageTransition::RELOAD &&
          load_details->type != NavigationType::SAME_PAGE) {
        return;
      }
      // When doing a page reload, we don't get a TAB_LANGUAGE_DETERMINED
      // notification.  So we need to explictly initiate the translation.
      // Note that we delay it as the TranslateManager gets this notification
      // before the TabContents and the TabContents processing might remove the
      // current infobars.  Since InitTranslation might add an infobar, it must
      // be done after that.
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
              &TranslateManager::InitiateTranslationPosted,
              controller->tab_contents()->render_view_host()->process()->id(),
              controller->tab_contents()->render_view_host()->routing_id(),
              controller->tab_contents()->language_state().
                  original_language()));
      break;
    }
    case NotificationType::TAB_LANGUAGE_DETERMINED: {
      TabContents* tab = Source<TabContents>(source).ptr();
      // We may get this notifications multiple times.  Make sure to translate
      // only once.
      LanguageState& language_state = tab->language_state();
      if (language_state.page_translatable() &&
          !language_state.translation_pending() &&
          !language_state.translation_declined() &&
          !language_state.IsPageTranslated()) {
        std::string language = *(Details<std::string>(details).ptr());
        InitiateTranslation(tab, language);
      }
      break;
    }
    case NotificationType::PAGE_TRANSLATED: {
      // Only add translate infobar if it doesn't exist; if it already exists,
      // just update the state, the actual infobar would have received the same
      //  notification and update the visual display accordingly.
      TabContents* tab = Source<TabContents>(source).ptr();
      PageTranslatedDetails* page_translated_details =
          Details<PageTranslatedDetails>(details).ptr();
      PageTranslated(tab, page_translated_details);
      break;
    }
    case NotificationType::PROFILE_DESTROYED: {
      Profile* profile = Source<Profile>(source).ptr();
      notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                     source);
      size_t count = accept_languages_.erase(profile->GetPrefs());
      // We should know about this profile since we are listening for
      // notifications on it.
      DCHECK(count > 0);
      pref_change_registrar_.Remove(prefs::kAcceptLanguages, this);
      break;
    }
    case NotificationType::PREF_CHANGED: {
      DCHECK(*Details<std::string>(details).ptr() == prefs::kAcceptLanguages);
      PrefService* prefs = Source<PrefService>(source).ptr();
      InitAcceptLanguages(prefs);
      break;
    }
    default:
      NOTREACHED();
  }
}

void TranslateManager::OnURLFetchComplete(const URLFetcher* source,
                                          const GURL& url,
                                          const net::URLRequestStatus& status,
                                          int response_code,
                                          const ResponseCookies& cookies,
                                          const std::string& data) {
  scoped_ptr<const URLFetcher> delete_ptr(source);
  DCHECK(translate_script_request_pending_);
  translate_script_request_pending_ = false;
  bool error =
      (status.status() != net::URLRequestStatus::SUCCESS ||
       response_code != 200);

  if (!error) {
    base::StringPiece str = ResourceBundle::GetSharedInstance().
        GetRawDataResource(IDR_TRANSLATE_JS);
    DCHECK(translate_script_.empty());
    str.CopyToString(&translate_script_);
    translate_script_ += "\n" + data;
    // We'll expire the cached script after some time, to make sure long running
    // browsers still get fixes that might get pushed with newer scripts.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &TranslateManager::ClearTranslateScript),
        translate_script_expiration_delay_);
  }

  // Process any pending requests.
  std::vector<PendingRequest>::const_iterator iter;
  for (iter = pending_requests_.begin(); iter != pending_requests_.end();
       ++iter) {
    const PendingRequest& request = *iter;
    TabContents* tab = tab_util::GetTabContentsByID(request.render_process_id,
                                                    request.render_view_id);
    if (!tab) {
      // The tab went away while we were retrieving the script.
      continue;
    }
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    if (!entry || entry->page_id() != request.page_id) {
      // We navigated away from the page the translation was triggered on.
      continue;
    }

    if (error) {
      ShowInfoBar(tab, TranslateInfoBarDelegate::CreateErrorDelegate(
          TranslateErrors::NETWORK, tab,
          request.source_lang, request.target_lang));
    } else {
      // Translate the page.
      DoTranslatePage(tab, translate_script_,
                      request.source_lang, request.target_lang);
    }
  }
  pending_requests_.clear();
}

// static
bool TranslateManager::IsShowingTranslateInfobar(TabContents* tab) {
  return GetTranslateInfoBarDelegate(tab) != NULL;
}

TranslateManager::TranslateManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      translate_script_expiration_delay_(kTranslateScriptExpirationDelayMS),
      translate_script_request_pending_(false) {
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::TAB_LANGUAGE_DETERMINED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
                              NotificationService::AllSources());
}

void TranslateManager::InitiateTranslation(TabContents* tab,
                                           const std::string& page_lang) {
  PrefService* prefs = tab->profile()->GetOriginalProfile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kEnableTranslate))
    return;

  pref_change_registrar_.Init(prefs);

  // Allow disabling of translate from the command line to assist with
  // automated browser testing.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableTranslate))
    return;

  NavigationEntry* entry = tab->controller().GetActiveEntry();
  if (!entry) {
    // This can happen for popups created with window.open("").
    return;
  }

  // If there is already a translate infobar showing, don't show another one.
  if (GetTranslateInfoBarDelegate(tab))
    return;

  std::string target_lang = GetTargetLanguage();
  // Nothing to do if either the language Chrome is in or the language of the
  // page is not supported by the translation server.
  if (target_lang.empty() || !IsSupportedLanguage(page_lang)) {
    return;
  }

  // We don't want to translate:
  // - any Chrome specific page (New Tab Page, Download, History... pages).
  // - similar languages (ex: en-US to en).
  // - any user black-listed URLs or user selected language combination.
  // - any language the user configured as accepted languages.
  if (!IsTranslatableURL(entry->url()) || page_lang == target_lang ||
      !TranslatePrefs::CanTranslate(prefs, page_lang, entry->url()) ||
      IsAcceptLanguage(tab, page_lang)) {
    return;
  }

  // If the user has previously selected "always translate" for this language we
  // automatically translate.  Note that in incognito mode we disable that
  // feature; the user will get an infobar, so they can control whether the
  // page's text is sent to the translate server.
  std::string auto_target_lang;
  if (!tab->profile()->IsOffTheRecord() &&
      TranslatePrefs::ShouldAutoTranslate(prefs, page_lang,
          &auto_target_lang)) {
    TranslatePage(tab, page_lang, auto_target_lang);
    return;
  }

  std::string auto_translate_to = tab->language_state().AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslatePage(tab, page_lang, auto_translate_to);
    return;
  }

  // Prompts the user if he/she wants the page translated.
  tab->AddInfoBar(TranslateInfoBarDelegate::CreateDelegate(
      TranslateInfoBarDelegate::BEFORE_TRANSLATE, tab, page_lang, target_lang));
}

void TranslateManager::InitiateTranslationPosted(
    int process_id, int render_id, const std::string& page_lang) {
  // The tab might have been closed.
  TabContents* tab = tab_util::GetTabContentsByID(process_id, render_id);
  if (!tab || tab->language_state().translation_pending())
    return;

  InitiateTranslation(tab, page_lang);
}

void TranslateManager::TranslatePage(TabContents* tab_contents,
                                      const std::string& source_lang,
                                      const std::string& target_lang) {
  NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  TranslateInfoBarDelegate* infobar = TranslateInfoBarDelegate::CreateDelegate(
      TranslateInfoBarDelegate::TRANSLATING, tab_contents,
      source_lang, target_lang);
  if (!infobar) {
    // This means the source or target languages are not supported, which should
    // not happen as we won't show a translate infobar or have the translate
    // context menu activated in such cases.
    NOTREACHED();
    return;
  }
  ShowInfoBar(tab_contents, infobar);

  if (!translate_script_.empty()) {
    DoTranslatePage(tab_contents, translate_script_, source_lang, target_lang);
    return;
  }

  // The script is not available yet.  Queue that request and query for the
  // script.  Once it is downloaded we'll do the translate.
  RenderViewHost* rvh = tab_contents->render_view_host();
  PendingRequest request;
  request.render_process_id = rvh->process()->id();
  request.render_view_id = rvh->routing_id();
  request.page_id = entry->page_id();
  request.source_lang = source_lang;
  request.target_lang = target_lang;
  pending_requests_.push_back(request);
  RequestTranslateScript();
}

void TranslateManager::RevertTranslation(TabContents* tab_contents) {
  NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }
  tab_contents->render_view_host()->Send(new ViewMsg_RevertTranslation(
      tab_contents->render_view_host()->routing_id(), entry->page_id()));
  tab_contents->language_state().set_current_language(
      tab_contents->language_state().original_language());
}

void TranslateManager::ReportLanguageDetectionError(TabContents* tab_contents) {
  UMA_HISTOGRAM_COUNTS("Translate.ReportLanguageDetectionError", 1);
  GURL page_url = tab_contents->controller().GetActiveEntry()->url();
  std::string report_error_url(kReportLanguageDetectionErrorURL);
  report_error_url += "?client=cr&action=langidc&u=";
  report_error_url += EscapeUrlEncodedData(page_url.spec());
  report_error_url += "&sl=";
  report_error_url += tab_contents->language_state().original_language();
  report_error_url += "&hl=";
  report_error_url +=
      GetLanguageCode(g_browser_process->GetApplicationLocale());
  // Open that URL in a new tab so that the user can tell us more.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser) {
    NOTREACHED();
    return;
  }
  browser->AddSelectedTabWithURL(GURL(report_error_url),
                                 PageTransition::AUTO_BOOKMARK);
}

void TranslateManager::DoTranslatePage(TabContents* tab,
                                       const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  tab->language_state().set_translation_pending(true);

  tab->render_view_host()->Send(new ViewMsg_TranslatePage(
      tab->render_view_host()->routing_id(), entry->page_id(), translate_script,
      source_lang, target_lang));

  // Ideally we'd have a better way to uniquely identify form control elements,
  // but we don't have that yet.  So before start translation, we clear the
  // current form and re-parse it in AutoFillManager first to get the new
  // labels.
  tab->autofill_manager()->Reset();
}

void TranslateManager::PageTranslated(TabContents* tab,
                                      PageTranslatedDetails* details) {
  // Create the new infobar to display.
  TranslateInfoBarDelegate* infobar;
  if (details->error_type != TranslateErrors::NONE) {
    infobar = TranslateInfoBarDelegate::CreateErrorDelegate(details->error_type,
        tab, details->source_language, details->target_language);
  } else if (!IsSupportedLanguage(details->source_language)) {
    // TODO(jcivelli): http://crbug.com/9390 We should change the "after
    //                 translate" infobar to support unknown as the original
    //                 language.
    UMA_HISTOGRAM_COUNTS("Translate.ServerReportedUnsupportedLanguage", 1);
    infobar = TranslateInfoBarDelegate::CreateErrorDelegate(
        TranslateErrors::UNSUPPORTED_LANGUAGE, tab,
        details->source_language, details->target_language);
  } else {
    infobar = TranslateInfoBarDelegate::CreateDelegate(
        TranslateInfoBarDelegate::AFTER_TRANSLATE, tab,
        details->source_language, details->target_language);
  }
  ShowInfoBar(tab, infobar);
}

bool TranslateManager::IsAcceptLanguage(TabContents* tab,
                                        const std::string& language) {
  PrefService* pref_service = tab->profile()->GetOriginalProfile()->GetPrefs();
  PrefServiceLanguagesMap::const_iterator iter =
      accept_languages_.find(pref_service);
  if (iter == accept_languages_.end()) {
    InitAcceptLanguages(pref_service);
    // Listen for this profile going away, in which case we would need to clear
    // the accepted languages for the profile.
    notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                                Source<Profile>(tab->profile()));
    // Also start listening for changes in the accept languages.
    pref_change_registrar_.Add(prefs::kAcceptLanguages, this);

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

void TranslateManager::RequestTranslateScript() {
  if (translate_script_request_pending_)
    return;

  translate_script_request_pending_ = true;
  URLFetcher* fetcher = URLFetcher::Create(0, GURL(kTranslateScriptURL),
                                           URLFetcher::GET, this);
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->set_extra_request_headers(kTranslateScriptHeader);
  fetcher->Start();
}

void TranslateManager::ShowInfoBar(TabContents* tab,
                                   TranslateInfoBarDelegate* infobar) {
  TranslateInfoBarDelegate* old_infobar = GetTranslateInfoBarDelegate(tab);
  infobar->UpdateBackgroundAnimation(old_infobar);
  if (old_infobar) {
    // There already is a translate infobar, simply replace it.
    tab->ReplaceInfoBar(old_infobar, infobar);
  } else {
    tab->AddInfoBar(infobar);
  }
}

// static
std::string TranslateManager::GetTargetLanguage() {
  std::string target_lang =
      GetLanguageCode(g_browser_process->GetApplicationLocale());
  return IsSupportedLanguage(target_lang) ? target_lang : std::string();
}

// static
TranslateInfoBarDelegate* TranslateManager::GetTranslateInfoBarDelegate(
    TabContents* tab) {
  for (size_t i = 0; i < tab->infobar_count(); ++i) {
    TranslateInfoBarDelegate* delegate =
        tab->GetInfoBarDelegateAt(i)->AsTranslateInfoBarDelegate();
    if (delegate)
      return delegate;
  }
  return NULL;
}
