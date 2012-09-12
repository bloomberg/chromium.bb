// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/prefs/pref_service.h"
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
#include "content/public/browser/web_contents.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser) {
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(prefs::kInstantEnabled, this);
  ResetInstant();
  browser_->tab_strip_model()->AddObserver(this);
}

BrowserInstantController::~BrowserInstantController() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool BrowserInstantController::OpenInstant(WindowOpenDisposition disposition) {
  // NEW_BACKGROUND_TAB results in leaving the omnibox open, so we don't attempt
  // to use the Instant preview.
  if (!instant() || !instant_->IsCurrent() || disposition == NEW_BACKGROUND_TAB)
    return false;

  if (disposition == CURRENT_TAB) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContents>(instant_->CommitCurrentPreview(
            INSTANT_COMMIT_PRESSED_ENTER)),
        content::NotificationService::NoDetails());
    return true;
  }

  if (disposition == NEW_FOREGROUND_TAB) {
    TabContents* preview = instant_->ReleasePreviewContents(
        INSTANT_COMMIT_PRESSED_ENTER);
    preview->web_contents()->GetController().PruneAllButActive();
    browser_->tab_strip_model()->AddTabContents(preview, -1,
        instant_->last_transition_type(), TabStripModel::ADD_ACTIVE);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContents>(preview),
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

void BrowserInstantController::ShowInstant() {
  // Call ShowInstant() first, before WasShown(), so that the preview is added
  // to the window hierarchy before it's painted. http://crbug.com/145568
  TabContents* preview = instant_->GetPreviewContents();
  browser_->window()->ShowInstant(preview);

  // TODO(beng): Investigate if we can avoid this and instead rely on the
  //             visibility of the WebContentsView.
  preview->web_contents()->WasShown();
  chrome::GetActiveWebContents(browser_)->WasHidden();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN,
      content::Source<InstantController>(instant()),
      content::NotificationService::NoDetails());
}

void BrowserInstantController::HideInstant() {
  browser_->window()->HideInstant();

  if (chrome::GetActiveWebContents(browser_))
    chrome::GetActiveWebContents(browser_)->WasShown();
  if (TabContents* preview = instant_->GetPreviewContents())
    preview->web_contents()->WasHidden();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_HIDDEN,
      content::Source<InstantController>(instant()),
      content::NotificationService::NoDetails());
}

void BrowserInstantController::CommitInstant(TabContents* preview) {
  TabContents* active_tab = chrome::GetActiveTabContents(browser_);
  int index = browser_->tab_strip_model()->GetIndexOfTabContents(active_tab);
  DCHECK_NE(TabStripModel::kNoTab, index);
  // TabStripModel takes ownership of |preview|.
  browser_->tab_strip_model()->ReplaceTabContentsAt(index, preview);
  // InstantUnloadHandler takes ownership of |active_tab|.
  instant_unload_handler_->RunUnloadListenersOrDestroy(active_tab, index);

  GURL url = preview->web_contents()->GetURL();
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

TabContents* BrowserInstantController::GetActiveTabContents() const {
  return chrome::GetActiveTabContents(browser_);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, content::NotificationObserver implementation:

void BrowserInstantController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PREF_CHANGED, type);
  DCHECK_EQ(std::string(prefs::kInstantEnabled),
            *content::Details<std::string>(details).ptr());
  ResetInstant();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, TabStripModelObserver implementation:

void BrowserInstantController::TabDeactivated(TabContents* contents) {
  if (instant())
    instant_->Hide();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, private:

void BrowserInstantController::ResetInstant() {
  instant_.reset(
      !browser_shutdown::ShuttingDownWithoutClosingBrowsers() &&
      browser_->is_type_tabbed() ?
          InstantController::CreateInstant(browser_->profile(), this) : NULL);
  instant_unload_handler_.reset(instant() ?
      new InstantUnloadHandler(browser_) : NULL);
}

}  // namespace chrome
