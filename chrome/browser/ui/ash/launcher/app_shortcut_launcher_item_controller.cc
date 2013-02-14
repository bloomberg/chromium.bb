// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_tab.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

using extensions::Extension;

// Item controller for an app shortcut. Shortcuts track app and launcher ids,
// but do not have any associated windows (opening a shortcut will replace the
// item with the appropriate LauncherItemController type).
AppShortcutLauncherItemController::AppShortcutLauncherItemController(
    const std::string& app_id,
    ChromeLauncherControllerPerApp* controller)
    : LauncherItemController(TYPE_SHORTCUT, app_id, controller),
      app_controller_(controller) {
  // To detect V1 applications we use their domain and match them against the
  // used URL. This will also work with applications like Google Drive.
  const Extension* extension =
      launcher_controller()->GetExtensionForAppID(app_id);
  // Some unit tests have no real extension and will set their
  if (extension)
    refocus_url_ = GURL(extension->launch_web_url() + "*");
}

AppShortcutLauncherItemController::~AppShortcutLauncherItemController() {
}

string16 AppShortcutLauncherItemController::GetTitle() {
  return GetAppTitle();
}

bool AppShortcutLauncherItemController::HasWindow(aura::Window* window) const {
  std::vector<content::WebContents*> content =
      app_controller_->GetV1ApplicationsFromAppId(app_id());
  for (size_t i = 0; i < content.size(); i++) {
    Browser* browser = chrome::FindBrowserWithWebContents(content[i]);
    if (browser && browser->window()->GetNativeWindow() == window)
      return true;
  }
  return false;
}

bool AppShortcutLauncherItemController::IsOpen() const {
  return !app_controller_->GetV1ApplicationsFromAppId(app_id()).empty();
}

void AppShortcutLauncherItemController::Launch(int event_flags) {
  app_controller_->LaunchApp(app_id(), event_flags);
}

void AppShortcutLauncherItemController::Activate() {
  std::vector<content::WebContents*> content = GetRunningApplications();
  if (content.empty()) {
    Launch(ui::EF_NONE);
    return;
  }
  Browser* browser = chrome::FindBrowserWithWebContents(content[0]);
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(content[0]);
  DCHECK_NE(TabStripModel::kNoTab, index);
  tab_strip->ActivateTabAt(index, false);
  browser->window()->Show();
  ash::wm::ActivateWindow(browser->window()->GetNativeWindow());
}

void AppShortcutLauncherItemController::Close() {
  // Close all running 'programs' of this type.
  std::vector<content::WebContents*> content =
      app_controller_->GetV1ApplicationsFromAppId(app_id());
  for (size_t i = 0; i < content.size(); i++) {
    Browser* browser = chrome::FindBrowserWithWebContents(content[i]);
    if (!browser)
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(content[i]);
    DCHECK(index != TabStripModel::kNoTab);
    tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_NONE);
  }
}

void AppShortcutLauncherItemController::Clicked(const ui::Event& event) {
  Activate();
}

void AppShortcutLauncherItemController::OnRemoved() {
  // AppShortcutLauncherItemController is unowned; delete on removal.
  delete this;
}

void AppShortcutLauncherItemController::LauncherItemChanged(
    int model_index,
    const ash::LauncherItem& old_item) {
}

ChromeLauncherAppMenuItems
AppShortcutLauncherItemController::GetApplicationList() {
  ChromeLauncherAppMenuItems items;
  // Add the application name to the menu.
  items.push_back(new ChromeLauncherAppMenuItem(GetTitle(), NULL));

  std::vector<content::WebContents*> content_list =
      GetRunningApplications();

  for (size_t i = 0; i < content_list.size(); i++) {
    content::WebContents* web_contents = content_list[i];
    // Get the icon.
    gfx::Image app_icon = app_controller_->GetAppListIcon(web_contents);
    items.push_back(new ChromeLauncherAppMenuItemTab(
        web_contents->GetTitle(),
        app_icon.IsEmpty() ? NULL : &app_icon,
        web_contents));
  }
  return items.Pass();
}

std::vector<content::WebContents*>
AppShortcutLauncherItemController::GetRunningApplications() {
  std::vector<content::WebContents*> items;

  URLPattern refocus_pattern(URLPattern::SCHEME_ALL);
  refocus_pattern.SetMatchAllURLs(true);

  if (!refocus_url_.is_empty()) {
    refocus_pattern.SetMatchAllURLs(false);
    refocus_pattern.Parse(refocus_url_.spec());
  }

  const Extension* extension =
      launcher_controller()->GetExtensionForAppID(app_id());

  const chrome::BrowserListImpl* ash_browser_list =
      chrome::BrowserListImpl::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (chrome::BrowserListImpl::const_reverse_iterator it =
           ash_browser_list->begin_last_active();
       it != ash_browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
    TabStripModel* tab_strip = browser->tab_strip_model();
    // We start to enumerate from the active index.
    int active_index = tab_strip->active_index();
    for (int index = 0; index  < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(
          (index + active_index) % tab_strip->count());
      const GURL tab_url = web_contents->GetURL();
      // There are three ways to identify the association of a URL with this
      // extension:
      // - The refocus pattern is matched (needed for apps like drive).
      // - The extension's origin + extent gets matched.
      // - The launcher controller knows that the tab got created for this app.
      if ((!refocus_pattern.match_all_urls() &&
           refocus_pattern.MatchesURL(tab_url)) ||
          (extension->OverlapsWithOrigin(tab_url) &&
           extension->web_extent().MatchesURL(tab_url)) ||
          launcher_controller()->GetPerAppInterface()->
             IsWebContentHandledByApplication(web_contents, app_id()))
        items.push_back(web_contents);
    }
  }
  return items;
}
