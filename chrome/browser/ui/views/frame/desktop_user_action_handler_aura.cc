// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_user_action_handler_aura.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

DesktopUserActionHandlerAura::DesktopUserActionHandlerAura(Browser* browser)
    : browser_(browser) {
}

DesktopUserActionHandlerAura::~DesktopUserActionHandlerAura() {}

bool DesktopUserActionHandlerAura::OnUserAction(
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

content::WebContents*
DesktopUserActionHandlerAura::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
