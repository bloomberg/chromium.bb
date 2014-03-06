// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_action_handler.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

// Returns the currently-active WebContents belonging to the active browser, or
// NULL if there's no currently-active browser.
content::WebContents* GetActiveWebContents() {
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(
      chrome::HOST_DESKTOP_TYPE_ASH);
  if (!browser)
    return NULL;
  if (!ash::wm::IsActiveWindow(browser->window()->GetNativeWindow()))
    return NULL;

  return browser->tab_strip_model()->GetActiveWebContents();
}

UserActionHandler::UserActionHandler() {}

UserActionHandler::~UserActionHandler() {}

bool UserActionHandler::OnUserAction(
    aura::client::UserActionClient::Command command) {
  switch (command) {
    case aura::client::UserActionClient::BACK: {
      content::WebContents* contents = GetActiveWebContents();
      if (contents && contents->GetController().CanGoBack()) {
        contents->GetController().GoBack();
        return true;
      }
      break;
    }
    case aura::client::UserActionClient::FORWARD: {
      content::WebContents* contents = GetActiveWebContents();
      if (contents && contents->GetController().CanGoForward()) {
        contents->GetController().GoForward();
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}
