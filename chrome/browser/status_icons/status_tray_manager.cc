// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_tray_manager.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"

StatusTrayManager::StatusTrayManager() {
}

StatusTrayManager::~StatusTrayManager() {
}

void StatusTrayManager::Init(Profile* profile) {
#if !(defined(OS_LINUX) && defined(TOOLKIT_VIEWS))
  DCHECK(profile);
  profile_ = profile;
  status_tray_.reset(StatusTray::Create());
  StatusIcon* icon = status_tray_->GetStatusIcon(ASCIIToUTF16("chrome_main"));
  if (icon) {
    // Create an icon and add ourselves as a click observer on it
    SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUS_TRAY_ICON);
    SkBitmap* pressed = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUS_TRAY_ICON_PRESSED);
    icon->SetImage(*bitmap);
    icon->SetPressedImage(*pressed);
    icon->SetToolTip(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    icon->AddObserver(this);
  }
#endif
}

void StatusTrayManager::OnClicked() {
  // When the tray icon is clicked, bring up the extensions page for now.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser) {
    // Bring up the existing browser window and show the extensions tab.
    browser->window()->Activate();
    browser->ShowExtensionsTab();
  } else {
    // No windows are currently open, so open a new one.
    Browser::OpenExtensionsWindow(profile_);
  }
}
