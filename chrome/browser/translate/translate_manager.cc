// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

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
      TabContents* tab = Source<TabContents>(source).ptr();
      std::pair<std::string, std::string>* language_pair =
          (Details<std::pair<std::string, std::string> >(details).ptr());
      PrefService* prefs = GetPrefService(tab);
      tab->AddInfoBar(new AfterTranslateInfoBarDelegate(tab, prefs,
                                                        language_pair->first,
                                                        language_pair->second));
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
  PrefService* prefs = GetPrefService(tab);
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  std::string chrome_lang = g_browser_process->GetApplicationLocale();
  chrome_lang = TranslationService::GetLanguageCode(chrome_lang);

  // We don't want to translate:
  // - any Chrome specific page (New Tab Page, Download, History... pages).
  // - similar languages (ex: en-US to en).
  // - any user black-listed URLs or user selected language combination.
  if (entry->url().SchemeIs("chrome") || page_lang == chrome_lang ||
      !TranslatePrefs::CanTranslate(prefs, page_lang, entry->url())) {
    return;
  }
  if (TranslatePrefs::ShouldAutoTranslate(prefs, page_lang, chrome_lang)) {
    // The user has previously select "always translate" for this language.
    tab->TranslatePage(page_lang, chrome_lang);
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
  tab->AddInfoBar(new BeforeTranslateInfoBarDelegate(tab, prefs,
                                                     entry->url(),
                                                     page_lang, chrome_lang));
}

PrefService* TranslateManager::GetPrefService(TabContents* tab) {
  PrefService* prefs = NULL;
  if (tab->profile())
    prefs = tab->profile()->GetPrefs();
  if (prefs)
    return prefs;

  return g_browser_process->local_state();
}
