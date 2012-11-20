// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
               chrome::search::IsInstantExtendedAPIEnabled(browser->profile())),
      instant_unload_handler_(browser) {
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(prefs::kInstantEnabled, this);
  instant_.SetInstantEnabled(IsInstantEnabled(browser_->profile()));
  browser_->search_model()->AddObserver(this);
}

BrowserInstantController::~BrowserInstantController() {
  browser_->search_model()->RemoveObserver(this);
}

bool BrowserInstantController::IsInstantEnabled(Profile* profile) {
  return profile && !profile->IsOffTheRecord() && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled);
}

void BrowserInstantController::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInstantConfirmDialogShown, false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kInstantEnabled, false,
                             PrefService::SYNCABLE_PREF);
}

bool BrowserInstantController::OpenInstant(WindowOpenDisposition disposition) {
  // Unsupported dispositions.
  if (disposition == NEW_BACKGROUND_TAB || disposition == NEW_WINDOW)
    return false;

  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this DCHECK file a bug and I'll (sky) add
  // support for the new disposition.
  DCHECK(disposition == CURRENT_TAB ||
         disposition == NEW_FOREGROUND_TAB) << disposition;

  return instant_.CommitIfCurrent(disposition == CURRENT_TAB ?
      INSTANT_COMMIT_PRESSED_ENTER : INSTANT_COMMIT_PRESSED_ALT_ENTER);
}

void BrowserInstantController::CommitInstant(TabContents* preview,
                                             bool in_new_tab) {
  if (in_new_tab) {
    // TabStripModel takes ownership of |preview|.
    browser_->tab_strip_model()->AddTabContents(preview, -1,
        instant_.last_transition_type(), TabStripModel::ADD_ACTIVE);
  } else {
    TabContents* active_tab =
        browser_->tab_strip_model()->GetActiveTabContents();
    int index = browser_->tab_strip_model()->GetIndexOfTabContents(active_tab);
    DCHECK_NE(TabStripModel::kNoTab, index);
    // TabStripModel takes ownership of |preview|.
    browser_->tab_strip_model()->ReplaceTabContentsAt(index, preview);
    // InstantUnloadHandler takes ownership of |active_tab|.
    instant_unload_handler_.RunUnloadListenersOrDestroy(active_tab, index);

    GURL url = preview->web_contents()->GetURL();
    DCHECK(browser_->profile()->GetExtensionService());
    if (browser_->profile()->GetExtensionService()->IsInstalledApp(url)) {
      AppLauncherHandler::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_OMNIBOX_INSTANT);
    }
  }
}

void BrowserInstantController::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
  if (browser_->window()->GetLocationBar())
    browser_->window()->GetLocationBar()->SetInstantSuggestion(suggestion);
}

gfx::Rect BrowserInstantController::GetInstantBounds() {
  return browser_->window()->GetInstantBounds();
}

void BrowserInstantController::InstantPreviewFocused() {
  // NOTE: This is only invoked on aura.
  browser_->window()->WebContentsFocused(
      instant_.GetPreviewContents()->web_contents());
}

TabContents* BrowserInstantController::GetActiveTabContents() const {
  return browser_->tab_strip_model()->GetActiveTabContents();
}

void BrowserInstantController::ActiveTabChanged() {
  instant_.ActiveTabChanged();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, PrefObserver implementation:

void BrowserInstantController::OnPreferenceChanged(
    PrefServiceBase* service,
    const std::string& pref_name) {
  instant_.SetInstantEnabled(IsInstantEnabled(browser_->profile()));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, search::SearchModelObserver implementation:

void BrowserInstantController::ModeChanged(const search::Mode& old_mode,
                                           const search::Mode& new_mode) {
  instant_.SearchModeChanged(old_mode, new_mode);
}

}  // namespace chrome
