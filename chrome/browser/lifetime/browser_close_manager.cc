// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/browser_close_manager.h"

#include "base/command_line.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"

BrowserCloseManager::BrowserCloseManager() : current_browser_(NULL) {}

BrowserCloseManager::~BrowserCloseManager() {}

void BrowserCloseManager::StartClosingBrowsers() {
  // If the session is ending or batch browser shutdown is disabled, skip
  // straight to closing the browsers. In the former case, there's no time to
  // wait for beforeunload dialogs; in the latter, the windows will manage
  // showing their own dialogs.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::END_SESSION ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBatchedShutdown)) {
    CloseBrowsers();
    return;
  }
  TryToCloseBrowsers();
}

void BrowserCloseManager::CancelBrowserClose() {
  browser_shutdown::SetTryingToQuit(false);
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    it->ResetBeforeUnloadHandlers();
  }
}

void BrowserCloseManager::TryToCloseBrowsers() {
  // If all browser windows can immediately be closed, fall out of this loop and
  // close the browsers. If any browser window cannot be closed, temporarily
  // stop closing. CallBeforeUnloadHandlers prompts the user and calls
  // OnBrowserReportCloseable with the result. If the user confirms the close,
  // this will trigger TryToCloseBrowsers to try again.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->CallBeforeUnloadHandlers(
            base::Bind(&BrowserCloseManager::OnBrowserReportCloseable, this))) {
      current_browser_ = *it;
      return;
    }
  }
  CloseBrowsers();
}

void BrowserCloseManager::OnBrowserReportCloseable(bool proceed) {
  if (!current_browser_)
    return;

  current_browser_ = NULL;

  if (proceed)
    TryToCloseBrowsers();
  else
    CancelBrowserClose();
}

void BrowserCloseManager::CloseBrowsers() {
  // Tell everyone that we are shutting down.
  browser_shutdown::SetTryingToQuit(true);

#if defined(ENABLE_SESSION_SERVICE)
  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();
#endif

  bool session_ending =
      browser_shutdown::GetShutdownType() == browser_shutdown::END_SESSION;
  for (scoped_ptr<chrome::BrowserIterator> it_ptr(
           new chrome::BrowserIterator());
       !it_ptr->done();) {
    Browser* browser = **it_ptr;
    browser->window()->Close();
    if (!session_ending) {
      it_ptr->Next();
    } else {
      // This path is hit during logoff/power-down. In this case we won't get
      // a final message and so we force the browser to be deleted.
      // Close doesn't immediately destroy the browser
      // (Browser::TabStripEmpty() uses invoke later) but when we're ending the
      // session we need to make sure the browser is destroyed now. So, invoke
      // DestroyBrowser to make sure the browser is deleted and cleanup can
      // happen.
      while (browser->tab_strip_model()->count())
        delete browser->tab_strip_model()->GetWebContentsAt(0);
      browser->window()->DestroyBrowser();
      it_ptr.reset(new chrome::BrowserIterator());
      if (!it_ptr->done() && browser == **it_ptr) {
        // Destroying the browser should have removed it from the browser list.
        // We should never get here.
        NOTREACHED();
        return;
      }
    }
  }
}
