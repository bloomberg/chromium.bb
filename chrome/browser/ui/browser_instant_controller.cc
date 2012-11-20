// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true iff the search model for |tab| is in an NTP state, that is, a
// state in which the Instant overlay may be showing custom NTP content in
// EXTENDED mode.
bool IsSearchModelNTP(content::WebContents* tab) {
  if (!tab)
    return false;
  chrome::search::SearchModel* model =
      chrome::search::SearchTabHelper::FromWebContents(tab)->model();
  return model && model->mode().is_ntp();
}

}  // namespace

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_unload_handler_(browser) {
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(prefs::kInstantEnabled, this);
  ResetInstant();
  browser_->tab_strip_model()->AddObserver(this);
  browser_->search_model()->AddObserver(this);
}

BrowserInstantController::~BrowserInstantController() {
  browser_->tab_strip_model()->RemoveObserver(this);
  browser_->search_model()->RemoveObserver(this);
}

bool BrowserInstantController::OpenInstant(WindowOpenDisposition disposition) {
  // NEW_BACKGROUND_TAB results in leaving the omnibox open, so we don't attempt
  // to use the Instant preview.
  if (!instant() || !instant_->IsCurrent() || disposition == NEW_BACKGROUND_TAB)
    return false;

  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this DCHECK file a bug and I'll (sky) add
  // support for the new disposition.
  DCHECK(disposition == CURRENT_TAB ||
         disposition == NEW_FOREGROUND_TAB) << disposition;

  instant_->CommitCurrentPreview(disposition == CURRENT_TAB ?
      INSTANT_COMMIT_PRESSED_ENTER : INSTANT_COMMIT_PRESSED_ALT_ENTER);
  return true;
}

void BrowserInstantController::CommitInstant(TabContents* preview,
                                             bool in_new_tab) {
  if (in_new_tab) {
    // TabStripModel takes ownership of |preview|.
    browser_->tab_strip_model()->AddTabContents(preview, -1,
        instant_->last_transition_type(), TabStripModel::ADD_ACTIVE);
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
      instant_->GetPreviewContents()->web_contents());
}

TabContents* BrowserInstantController::GetActiveTabContents() const {
  return browser_->tab_strip_model()->GetActiveTabContents();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, PrefObserver implementation:

void BrowserInstantController::OnPreferenceChanged(
    PrefServiceBase* service,
    const std::string& pref_name) {
  DCHECK_EQ(std::string(prefs::kInstantEnabled), pref_name);
  ResetInstant();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, TabStripModelObserver implementation:

void BrowserInstantController::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    bool user_gesture) {
  if (instant()) {
    const bool old_is_ntp = IsSearchModelNTP(old_contents);
    const bool new_is_ntp = IsSearchModelNTP(new_contents);
    // Do not hide Instant if switching from an NTP to another NTP since that
    // would cause custom NTP content to flicker.
    if (!(old_is_ntp && new_is_ntp))
      instant()->Hide();
  }
}

void BrowserInstantController::TabStripEmpty() {
  instant_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, search::SearchModelObserver implementation:

void BrowserInstantController::ModeChanged(const search::Mode& old_mode,
                                           const search::Mode& new_mode) {
  if (instant())
    instant_->OnActiveTabModeChanged(new_mode);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, private:

void BrowserInstantController::ResetInstant() {
  instant_.reset(
      !browser_shutdown::ShuttingDownWithoutClosingBrowsers() &&
      browser_->is_type_tabbed() ?
          InstantController::CreateInstant(browser_->profile(), this) : NULL);

  // Notify any observers that they need to reset.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_INSTANT_RESET,
      content::Source<BrowserInstantController>(this),
      content::NotificationService::NoDetails());
}

}  // namespace chrome
