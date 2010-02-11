// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/translation_service.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

TranslateManager::~TranslateManager() {
}

void TranslateManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED: {
      // We have navigated to a new page.
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      NavigationEntry* entry = controller->GetActiveEntry();
      if (!entry->language().empty()) {
        // The language for that page is known (it must be a back/forward
        // navigation to a page we already visited).
        InitiateTranslation(controller->tab_contents(), entry->language());
      }
      break;
    }
    case NotificationType::TAB_LANGUAGE_DETERMINED: {
      TabContents* tab = Source<TabContents>(source).ptr();
      std::string language = *(Details<std::string>(details).ptr());
      InitiateTranslation(tab, language);
      break;
    }
    case NotificationType::PAGE_TRANSLATED: {
      // Only add translate infobar if it doesn't exist; if it already exists,
      // it would have received the same notification and acted accordingly.
      TabContents* tab = Source<TabContents>(source).ptr();
      int i;
      for (i = 0; i < tab->infobar_delegate_count(); ++i) {
        InfoBarDelegate* info_bar = tab->GetInfoBarDelegateAt(i);
        if (info_bar->AsTranslateInfoBarDelegate())
          break;
      }
      if (i == tab->infobar_delegate_count()) {
        NavigationEntry* entry = tab->controller().GetActiveEntry();
        if (entry) {
          std::pair<std::string, std::string>* language_pair =
              (Details<std::pair<std::string, std::string> >(details).ptr());
          PrefService* prefs = tab->profile()->GetPrefs();
          tab->AddInfoBar(new TranslateInfoBarDelegate(tab, prefs,
              TranslateInfoBarDelegate::kAfterTranslate, entry->url(),
              language_pair->first, language_pair->second));
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

TranslateManager::TranslateManager() {
  if (!TranslationService::IsTranslationEnabled())
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
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  if (!TranslationService::IsSupportedLanguage(page_lang))
    return;  // Nothing to do, we don't support that language.

  std::string ui_lang = TranslationService::GetLanguageCode(
      g_browser_process->GetApplicationLocale());

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

  // If we already have an "after translate" infobar, it sometimes might be
  // sticky when running in frames.  So we need to proactively remove any
  // translate related infobars as they would prevent any new infobar from
  // showing. (As TabContents will not add an infobar if there is already one
  // showing equal to the one being added.)
  for (int i = 0; i < tab->infobar_delegate_count(); ++i) {
    InfoBarDelegate* info_bar = tab->GetInfoBarDelegateAt(i);
    if (info_bar->AsTranslateInfoBarDelegate())
      tab->RemoveInfoBar(info_bar);
  }

  // Prompts the user if he/she wants the page translated.
  tab->AddInfoBar(new TranslateInfoBarDelegate(tab, prefs,
      TranslateInfoBarDelegate::kBeforeTranslate, entry->url(),
      page_lang, ui_lang));
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
  for (iter = accept_langs_list.begin();
       iter != accept_langs_list.end(); ++iter) {
    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    std::string accept_lang(*iter);
    size_t index = iter->find("-");
    if (index != std::string::npos && *iter != "zh-CN" && *iter != "zh-TW")
      accept_lang = iter->substr(0, iter->length() - index - 1);
    accept_langs_set.insert(accept_lang);
  }
  accept_languages_[prefs] = accept_langs_set;
}
