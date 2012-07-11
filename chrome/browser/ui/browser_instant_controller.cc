// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"


namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser) {
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(prefs::kInstantEnabled, this);
  CreateInstantIfNecessary();
  browser_->tab_strip_model()->AddObserver(this);
}

BrowserInstantController::~BrowserInstantController() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool BrowserInstantController::OpenInstant(WindowOpenDisposition disposition) {
  if (!instant() || !instant()->PrepareForCommit() ||
      disposition == NEW_BACKGROUND_TAB) {
    // NEW_BACKGROUND_TAB results in leaving the omnibox open, so we don't
    // attempt to use the instant preview.
    return false;
  }

  if (disposition == CURRENT_TAB) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContents>(instant()->CommitCurrentPreview(
            INSTANT_COMMIT_PRESSED_ENTER)),
        content::NotificationService::NoDetails());
    return true;
  }
  if (disposition == NEW_FOREGROUND_TAB) {
    TabContents* preview_contents = instant()->ReleasePreviewContents(
        INSTANT_COMMIT_PRESSED_ENTER, NULL);
    // HideInstant is invoked after release so that InstantController is not
    // active when HideInstant asks it for its state.
    HideInstant();
    preview_contents->web_contents()->GetController().PruneAllButActive();
    browser_->tab_strip_model()->AddTabContents(
        preview_contents,
        -1,
        instant()->last_transition_type(),
        TabStripModel::ADD_ACTIVE);
    instant()->CompleteRelease(preview_contents);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContents>(preview_contents),
        content::NotificationService::NoDetails());
    return true;
  }
  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this NOTREACHED file a bug and I'll (sky) add
  // support for the new disposition.
  NOTREACHED();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, InstantControllerDelegate implementation:

void BrowserInstantController::ShowInstant(TabContents* preview_contents) {
  browser_->window()->ShowInstant(preview_contents);

  // TODO(beng): investigate if we can avoid this and instead rely on the
  //             visibility of the WebContentsView
  chrome::GetActiveWebContents(browser_)->WasHidden();
  preview_contents->web_contents()->WasRestored();
}

void BrowserInstantController::HideInstant() {
  browser_->window()->HideInstant();
  if (chrome::GetActiveWebContents(browser_))
    chrome::GetActiveWebContents(browser_)->WasRestored();
  if (instant_->GetPreviewContents())
    instant_->GetPreviewContents()->web_contents()->WasHidden();
}

void BrowserInstantController::CommitInstant(TabContents* preview_contents) {
  TabContents* tab_contents = chrome::GetActiveTabContents(browser_);
  int index = browser_->tab_strip_model()->GetIndexOfTabContents(tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  // TabStripModel takes ownership of preview_contents.
  browser_->tab_strip_model()->ReplaceTabContentsAt(index, preview_contents);
  // InstantUnloadHandler takes ownership of tab_contents.
  instant_unload_handler_->RunUnloadListenersOrDestroy(tab_contents, index);

  GURL url = preview_contents->web_contents()->GetURL();
  DCHECK(browser_->profile()->GetExtensionService());
  if (browser_->profile()->GetExtensionService()->IsInstalledApp(url)) {
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_INSTANT);
  }
}

void BrowserInstantController::SetSuggestedText(
    const string16& text,
    InstantCompleteBehavior behavior) {
  if (browser_->window()->GetLocationBar())
    browser_->window()->GetLocationBar()->SetSuggestedText(text, behavior);
}

gfx::Rect BrowserInstantController::GetInstantBounds() {
  return browser_->window()->GetInstantBounds();
}

void BrowserInstantController::InstantPreviewFocused() {
  // NOTE: This is only invoked on aura.
  browser_->window()->WebContentsFocused(
      instant_->GetPreviewContents()->web_contents());
}

TabContents* BrowserInstantController::GetInstantHostTabContents() const {
  return chrome::GetActiveTabContents(browser_);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, content::NotificationObserver implementation:

void BrowserInstantController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  const std::string& pref_name =
      *content::Details<std::string>(details).ptr();
  DCHECK(pref_name == prefs::kInstantEnabled);
  if (browser_shutdown::ShuttingDownWithoutClosingBrowsers() ||
      !InstantController::IsEnabled(browser_->profile())) {
    if (instant()) {
      instant()->DestroyPreviewContents();
      instant_.reset();
      instant_unload_handler_.reset();
    }
  } else {
    CreateInstantIfNecessary();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, TabStripModelObserver implementation:

void BrowserInstantController::TabDeactivated(TabContents* contents) {
  if (instant())
    instant()->Hide();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, private:

void BrowserInstantController::CreateInstantIfNecessary() {
  if (browser_->is_type_tabbed() &&
      InstantController::IsEnabled(browser_->profile()) &&
      !browser_->profile()->IsOffTheRecord()) {
    instant_.reset(new InstantController(this, InstantController::INSTANT));
    instant_unload_handler_.reset(new InstantUnloadHandler(browser_));
  }
}

}  // namespace chrome
