// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/spellchecker_submenu_observer.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellchecker_platform_engine.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/spelling_bubble_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

using content::BrowserThread;

SpellCheckerSubMenuObserver::SpellCheckerSubMenuObserver(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel::Delegate* delegate,
    int group)
    : proxy_(proxy),
      submenu_model_(delegate),
      spellcheck_enabled_(false),
      integrate_spelling_service_(false),
      language_group_(group),
      language_selected_(0) {
  DCHECK(proxy_);
}

SpellCheckerSubMenuObserver::~SpellCheckerSubMenuObserver() {
}

void SpellCheckerSubMenuObserver::InitMenu(const ContextMenuParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  spellcheck_enabled_ = params.spellcheck_enabled;

  // Add available spell-checker languages to the sub menu.
  Profile* profile = proxy_->GetProfile();
  language_selected_ =
      SpellCheckHost::GetSpellCheckLanguages(profile, &languages_);
  DCHECK(languages_.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < languages_.size(); ++i) {
    string16 display_name(
        l10n_util::GetDisplayNameForLocale(languages_[i], app_locale, true));
    submenu_model_.AddRadioItem(IDC_SPELLCHECK_LANGUAGES_FIRST + i,
                                display_name,
                                language_group_);
  }

  // Add an item that opens the 'fonts and languages options' page.
  submenu_model_.AddSeparator();
  submenu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS,
      IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);

  // Add a 'Check the spelling of this field' item in the sub menu.
  submenu_model_.AddCheckItem(
      IDC_CHECK_SPELLING_OF_THIS_FIELD,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_CHECK_SPELLING_OF_THIS_FIELD));

  // Add an item that shows the spelling panel if the platform spellchecker
  // supports it.
  if (SpellCheckerPlatform::SpellCheckerAvailable() &&
      SpellCheckerPlatform::SpellCheckerProvidesPanel()) {
    submenu_model_.AddCheckItem(
        IDC_SPELLPANEL_TOGGLE,
        l10n_util::GetStringUTF16(
            SpellCheckerPlatform::SpellingPanelVisible() ?
                IDS_CONTENT_CONTEXT_HIDE_SPELLING_PANEL :
                IDS_CONTENT_CONTEXT_SHOW_SPELLING_PANEL));
  }

#if defined(OS_WIN)
  // If we have not integrated the spelling service, we show an "Ask Google for
  // spelling suggestions" item. On the other hand, if we have integrated the
  // spelling service, we show "Stop asking Google for spelling suggestions"
  // item.
  integrate_spelling_service_ =
      profile->GetPrefs()->GetBoolean(prefs::kSpellCheckUseSpellingService);
  int spelling_message = integrate_spelling_service_ ?
      IDS_CONTENT_CONTEXT_SPELLING_STOP_ASKING_GOOGLE :
      IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE;
  submenu_model_.AddCheckItem(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                              l10n_util::GetStringUTF16(spelling_message));
#endif

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

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
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

  // Check box for 'Check the Spelling of this field'.
  if (command_id == IDC_CHECK_SPELLING_OF_THIS_FIELD) {
    Profile* profile = proxy_->GetProfile();
    if (!profile || !profile->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck))
      return false;
    return spellcheck_enabled_;
  }

  return false;
}

bool SpellCheckerSubMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  Profile* profile = proxy_->GetProfile();
  if (!profile)
    return false;

  const PrefService* pref = profile->GetPrefs();
  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    return pref->GetBoolean(prefs::kEnableSpellCheck);
  }

  switch (command_id) {
    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      return pref->GetBoolean(prefs::kEnableSpellCheck);

    case IDC_SPELLPANEL_TOGGLE:
    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return true;
  }

  return false;
}

void SpellCheckerSubMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  // Check to see if one of the spell check language ids have been clicked.
  if (command_id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      command_id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    Profile* profile = proxy_->GetProfile();
    const size_t language = command_id - IDC_SPELLCHECK_LANGUAGES_FIRST;
    if (profile && language < languages_.size()) {
      StringPrefMember dictionary_language;
      dictionary_language.Init(prefs::kSpellCheckDictionary,
                               profile->GetPrefs(),
                               NULL);
      dictionary_language.SetValue(languages_[language]);
    }
    return;
  }

  RenderViewHost* rvh = proxy_->GetRenderViewHost();
  switch (command_id) {
    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      rvh->Send(new SpellCheckMsg_ToggleSpellCheck(rvh->routing_id()));
      break;

    case IDC_SPELLPANEL_TOGGLE:
      rvh->Send(new SpellCheckMsg_ToggleSpellPanel(
          rvh->routing_id(), SpellCheckerPlatform::SpellingPanelVisible()));
      break;

#if defined(OS_WIN)
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      // When a user chooses the "Ask Google for spelling suggestions" item, we
      // show a bubble to confirm it. On the other hand, when a user chooses the
      // "Stop asking Google for spelling suggestions" item, we directly update
      // the profile and stop integrating the spelling service immediately.
      if (!integrate_spelling_service_) {
        gfx::Rect rect = rvh->view()->GetViewBounds();
        ConfirmBubbleModel::Show(rvh->view()->GetNativeView(),
                                 gfx::Point(rect.CenterPoint().x(), rect.y()),
                                 new SpellingBubbleModel(proxy_->GetProfile()));
      } else {
        Profile* profile = proxy_->GetProfile();
        if (profile)
          profile->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService,
                                          false);
      }
      break;
#endif
  }
}
