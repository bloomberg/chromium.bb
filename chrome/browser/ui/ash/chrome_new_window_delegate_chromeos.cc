// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_delegate_chromeos.h"

#include "apps/shell_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "ash/keyboard_overlay/keyboard_overlay_view.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

ChromeNewWindowDelegateChromeos::ChromeNewWindowDelegateChromeos() {}
ChromeNewWindowDelegateChromeos::~ChromeNewWindowDelegateChromeos() {}

void ChromeNewWindowDelegateChromeos::OpenFileManager() {
  using file_manager::kFileManagerAppId;
  Profile* const profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  const ExtensionService* const service = profile->GetExtensionService();
  if (service == NULL ||
      !service->IsExtensionEnabledForLauncher(kFileManagerAppId))
    return;
  const extensions::Extension* const extension =
      service->GetInstalledExtension(kFileManagerAppId);
  // event_flags = 0 means this invokes the same behavior as the launcher
  // item is clicked without any keyboard modifiers.
  OpenApplication(AppLaunchParams(profile,
                                  extension,
                                  0 /* event_flags */,
                                  chrome::HOST_DESKTOP_TYPE_ASH));
}

void ChromeNewWindowDelegateChromeos::OpenCrosh() {
  GURL crosh_url = extensions::TerminalExtensionHelper::GetCroshExtensionURL(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
  if (!crosh_url.is_valid())
    return;
  Browser* browser = GetTargetBrowser();
  content::WebContents* page = browser->OpenURL(
      content::OpenURLParams(crosh_url,
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_GENERATED,
                             false));
  browser->window()->Show();
  browser->window()->Activate();
  page->GetView()->Focus();
}

void ChromeNewWindowDelegateChromeos::ShowKeyboardOverlay() {
  // TODO(mazda): Move the show logic to ash (http://crbug.com/124222).
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  std::string url(chrome::kChromeUIKeyboardOverlayURL);
  ash::KeyboardOverlayView::ShowDialog(profile,
                                       new ChromeWebContentsHandler,
                                       GURL(url));
}
