// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spellchecker_submenu_observer.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

using content::BrowserThread;

SpellCheckerSubMenuObserver::SpellCheckerSubMenuObserver(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel::Delegate* delegate,
    int group)
    : proxy_(proxy),
      submenu_model_(delegate) {
  DCHECK(proxy_);
}

SpellCheckerSubMenuObserver::~SpellCheckerSubMenuObserver() {
}

void SpellCheckerSubMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Add an item that toggles the spelling panel.
  submenu_model_.AddCheckItem(
      IDC_SPELLPANEL_TOGGLE,
      l10n_util::GetStringUTF16(
          spellcheck_mac::SpellingPanelVisible() ?
              IDS_CONTENT_CONTEXT_HIDE_SPELLING_PANEL :
              IDS_CONTENT_CONTEXT_SHOW_SPELLING_PANEL));
  submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  // Add a 'Check Spelling While Typing' item in the sub menu.
  submenu_model_.AddCheckItem(
      IDC_CHECK_SPELLING_WHILE_TYPING,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_CHECK_SPELLING_WHILE_TYPING));

  proxy_->AddSubMenu(
      IDC_SPELLCHECK_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU),
      &submenu_model_);
}

bool SpellCheckerSubMenuObserver::IsCommandIdSupported(int command_id) {
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      // Return false so RenderViewContextMenu can handle this item because it
      // is hard for this class to handle it.
      return false;

    case IDC_CHECK_SPELLING_WHILE_TYPING:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return true;
  }

  return false;
}

bool SpellCheckerSubMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  // Check box for 'Check Spelling while typing'.
  if (command_id == IDC_CHECK_SPELLING_WHILE_TYPING) {
    Profile* profile = proxy_->GetProfile();
    DCHECK(profile);
    return profile->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck);
  }

  return false;
}

bool SpellCheckerSubMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return true;
  }

  return false;
}

void SpellCheckerSubMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  content::RenderViewHost* rvh = proxy_->GetRenderViewHost();
  Profile* profile = proxy_->GetProfile();
  DCHECK(profile);
  switch (command_id) {
    case IDC_CHECK_SPELLING_WHILE_TYPING:
      profile->GetPrefs()->SetBoolean(
          prefs::kEnableContinuousSpellcheck,
          !profile->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck));
      break;

    case IDC_SPELLPANEL_TOGGLE:
      rvh->Send(new SpellCheckMsg_ToggleSpellPanel(
          rvh->GetRoutingID(), spellcheck_mac::SpellingPanelVisible()));
      break;
  }
}
