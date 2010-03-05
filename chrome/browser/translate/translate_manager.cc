// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/translation_service.h"
#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

// static
bool TranslateManager::test_enabled_ = false;

TranslateManager::~TranslateManager() {
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
      std::string language = *(Details<std::string>(details).ptr());
      // We may get this notifications multiple times.  Make sure to translate
      // only once.
      LanguageState& language_state = tab->language_state();
      if (!language_state.translation_pending() &&
          !language_state.translation_declined() &&
          !language_state.IsPageTranslated()) {
        InitiateTranslation(tab, language);
      }
      break;
    }
    case NotificationType::PAGE_TRANSLATED: {
      // Only add translate infobar if it doesn't exist; if it already exists,
      // just update the state, the actual infobar would have received the same
      //  notification and update the visual display accordingly.
      TabContents* tab = Source<TabContents>(source).ptr();
      int i;
      for (i = 0; i < tab->infobar_delegate_count(); ++i) {
        TranslateInfoBarDelegate* info_bar =
            tab->GetInfoBarDelegateAt(i)->AsTranslateInfoBarDelegate();
        if (info_bar) {
          info_bar->UpdateState(TranslateInfoBarDelegate::kAfterTranslate);
          break;
        }
      }
      if (i == tab->infobar_delegate_count()) {
        NavigationEntry* entry = tab->controller().GetActiveEntry();
        if (entry) {
          std::pair<std::string, std::string>* language_pair =
              (Details<std::pair<std::string, std::string> >(details).ptr());
          AddTranslateInfoBar(tab, TranslateInfoBarDelegate::kAfterTranslate,
                              entry->url(),
                              language_pair->first, language_pair->second);
        }
      }
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
      profile->GetPrefs()->RemovePrefObserver(prefs::kAcceptLanguages, this);
      break;
    }
    case NotificationType::PREF_CHANGED: {
      DCHECK(*Details<std::wstring>(details).ptr() == prefs::kAcceptLanguages);
      PrefService* prefs = Source<PrefService>(source).ptr();
      InitAcceptLanguages(prefs);
      break;
    }
    default:
      NOTREACHED();
  }
}

TranslateManager::TranslateManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  if (!test_enabled_ && !TranslationService::IsTranslationEnabled())
    return;

  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::TAB_LANGUAGE_DETERMINED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
                              NotificationService::AllSources());
}

void TranslateManager::InitiateTranslation(TabContents* tab,
                                           const std::string& page_lang) {
  PrefService* prefs = tab->profile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kEnableTranslate))
    return;

  NavigationEntry* entry = tab->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  std::string ui_lang = TranslationService::GetLanguageCode(
      g_browser_process->GetApplicationLocale());

  // Nothing to do if the language Chrome is in or the language of the page is
  // not supported by the translation server.
  if (!TranslationService::IsSupportedLanguage(ui_lang) ||
      !TranslationService::IsSupportedLanguage(page_lang)) {
    return;
  }

  // We don't want to translate:
  // - any Chrome specific page (New Tab Page, Download, History... pages).
  // - similar languages (ex: en-US to en).
  // - any user black-listed URLs or user selected language combination.
  // - any language the user configured as accepted languages.
  if (entry->url().SchemeIs("chrome") || page_lang == ui_lang ||
      !TranslatePrefs::CanTranslate(prefs, page_lang, entry->url()) ||
      IsAcceptLanguage(tab, page_lang)) {
    return;
  }

  if (TranslatePrefs::ShouldAutoTranslate(prefs, page_lang, ui_lang)) {
    // The user has previously select "always translate" for this language.
    tab->TranslatePage(page_lang, ui_lang);
    return;
  }

  std::string auto_translate_to = tab->language_state().AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    tab->TranslatePage(page_lang, auto_translate_to);
    return;
  }

  // Prompts the user if he/she wants the page translated.
  AddTranslateInfoBar(tab, TranslateInfoBarDelegate::kBeforeTranslate,
                      entry->url(), page_lang, ui_lang);
}

void TranslateManager::InitiateTranslationPosted(int process_id,
                                                 int render_id,
                                                 const std::string& page_lang) {
  // The tab might have been closed.
  TabContents* tab = tab_util::GetTabContentsByID(process_id, render_id);
  if (!tab || tab->language_state().translation_pending())
    return;

  InitiateTranslation(tab, page_lang);
}

bool TranslateManager::IsAcceptLanguage(TabContents* tab,
                                        const std::string& language) {
  PrefService* pref_service = tab->profile()->GetPrefs();
  PrefServiceLanguagesMap::const_iterator iter =
      accept_languages_.find(pref_service);
  if (iter == accept_languages_.end()) {
    InitAcceptLanguages(pref_service);
    // Listen for this profile going away, in which case we would need to clear
    // the accepted languages for the profile.
    notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                                Source<Profile>(tab->profile()));
    // Also start listening for changes in the accept languages.
    tab->profile()->GetPrefs()->AddPrefObserver(prefs::kAcceptLanguages, this);

    iter = accept_languages_.find(pref_service);
  }

  return iter->second.count(language) != 0;
}

void TranslateManager::InitAcceptLanguages(PrefService* prefs) {
  // We have been asked for this profile, build the languages.
  std::wstring accept_langs_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_langs_list;
  LanguageSet accept_langs_set;
  SplitString(WideToASCII(accept_langs_str), ',', &accept_langs_list);
  std::vector<std::string>::const_iterator iter;
  std::string ui_lang = TranslationService::GetLanguageCode(
      g_browser_process->GetApplicationLocale());
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

void TranslateManager::AddTranslateInfoBar(
    TabContents* tab, TranslateInfoBarDelegate::TranslateState state,
    const GURL& url,
    const std::string& original_language, const std::string& target_language) {
  PrefService* prefs = tab->profile()->GetPrefs();
  TranslateInfoBarDelegate* infobar =
      TranslateInfoBarDelegate::Create(tab, prefs, state, url,
                                       original_language, target_language);
  if (!infobar) {
    NOTREACHED() << "Failed to create infobar for language " <<
        original_language << " and " << target_language;
    return;
  }
  tab->AddInfoBar(infobar);
}
