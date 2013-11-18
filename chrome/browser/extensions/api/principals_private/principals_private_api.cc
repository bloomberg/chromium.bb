// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/principals_private/principals_private_api.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

namespace extensions {

bool PrincipalsPrivateExtensionFunction::RunImpl() {
  if (!profiles::IsNewProfileManagementEnabled()) {
    SetError(
        "Need to enable new-profile-management to use principalsPrivate API.");
    return false;
  }
  return RunImplSafe();
}

bool PrincipalsPrivateSignOutFunction::RunImplSafe() {
  Browser* browser = GetCurrentBrowser();
  if (browser) {
    AvatarMenu avatar_menu(
        &g_browser_process->profile_manager()->GetProfileInfoCache(), NULL,
        browser);
    avatar_menu.BeginSignOut();
  }
  return true;
}

bool PrincipalsPrivateShowAvatarBubbleFunction::RunImplSafe() {
  Browser* browser = GetCurrentBrowser();
  if (browser)
    browser->window()->ShowAvatarBubbleFromAvatarButton();
  return true;
}

}  // namespace extensions
