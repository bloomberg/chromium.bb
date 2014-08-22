// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spellchecker_submenu_observer.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

using content::BrowserThread;

SpellCheckerSubMenuObserver::SpellCheckerSubMenuObserver(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel::Delegate* delegate,
    int group)
    : proxy_(proxy),
      submenu_model_(delegate),
      language_group_(group),
      language_selected_(0) {
  DCHECK(proxy_);
}

SpellCheckerSubMenuObserver::~SpellCheckerSubMenuObserver() {
}

void SpellCheckerSubMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Add available spell-checker languages to the sub menu.
  content::BrowserContext* browser_context = proxy_->GetBrowserContext();
  DCHECK(browser_context);
  language_selected_ =
      SpellcheckService::GetSpellCheckLanguages(browser_context, &languages_);
  DCHECK(languages_.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < languages_.size(); ++i) {
    base::string16 display_name(
        l10n_util::GetDisplayNameForLocale(languages_[i], app_locale, true));
    submenu_model_.AddRadioItem(IDC_SPELLCHECK_LANGUAGES_FIRST + i,
                                display_name,
                                language_group_);
  }

  // Add an item that opens the 'fonts and languages options' page.
  submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  submenu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS,
      IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);

  // Add a 'Check spelling while typing' item in the sub menu.
  submenu_model_.AddCheckItem(
      IDC_CHECK_SPELLING_WHILE_TYPING,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_CHECK_SPELLING_WHILE_TYPING));

  // Add a check item "Ask Google for spelling suggestions" item. (This class
  // does not handle this item because the SpellingMenuObserver class handles it
  // on behalf of this class.)
  submenu_model_.AddCheckItem(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE));

  // Add a check item "Automatically correct spelling".
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSpellingAutoCorrect)) {
    submenu_model_.AddCheckItem(IDC_CONTENT_CONTEXT_AUTOCORRECT_SPELLING_TOGGLE,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_AUTOCORRECT));
  }

  proxy_->AddSubMenu(
      IDC_SPELLCHECK_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU),
      &submenu_model_);
}

bool SpellCheckerSubMenuObserver::IsCommandIdSupported(int command_id) {
  // Allow Spell Check language items on sub menu for text area context menu.
  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    return true;
  }

  switch (command_id) {
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      // Return false so RenderViewContextMenu can handle this item because it
      // is hard for this class to handle it.
      return false;

    case IDC_CHECK_SPELLING_WHILE_TYPING:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_SPELLCHECK_MENU:
      return true;
  }

  return false;
}

bool SpellCheckerSubMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    return language_selected_ == command_id - IDC_SPELLCHECK_LANGUAGES_FIRST;
  }

  // Check box for 'Check Spelling while typing'.
  if (command_id == IDC_CHECK_SPELLING_WHILE_TYPING) {
    Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
    DCHECK(profile);
    return profile->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck);
  }

  return false;
}

bool SpellCheckerSubMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile);
  const PrefService* pref = profile->GetPrefs();
  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    return pref->GetBoolean(prefs::kEnableContinuousSpellcheck);
  }

  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_SPELLCHECK_MENU:
      return true;
  }

  return false;
}

void SpellCheckerSubMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  // Check to see if one of the spell check language ids have been clicked.
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  DCHECK(profile);
  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    const size_t language = command_id - IDC_SPELLCHECK_LANGUAGES_FIRST;
    if (profile && language < languages_.size()) {
      StringPrefMember dictionary_language;
      dictionary_language.Init(prefs::kSpellCheckDictionary,
                               profile->GetPrefs());
      dictionary_language.SetValue(languages_[language]);
    }
    return;
  }

  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING:
      profile->GetPrefs()->SetBoolean(
          prefs::kEnableContinuousSpellcheck,
          !profile->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck));
    break;
  }
}
