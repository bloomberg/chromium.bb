// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"

#include <stddef.h>

#include "ash/public/cpp/shelf_application_menu_item.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_animations.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

// The time delta between clicks in which clicks to launch V2 apps are ignored.
const int kClickSuppressionInMS = 1000;

// Check if a browser can be used for activation. This addresses a special use
// case in the M31 multi profile mode where a user activates a V1 app which only
// exists yet on another users desktop, but they expect to get only their own
// app items and not the ones from other users through activation.
// TODO(skuhne): Remove this function and replace the call with
// launcher_controller()->IsBrowserFromActiveUser(browser) once this experiment
// goes away.
bool CanBrowserBeUsedForDirectActivation(Browser* browser,
                                         ChromeLauncherController* launcher) {
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() ==
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_OFF)
    return true;
  return multi_user_util::IsProfileFromActiveUser(browser->profile());
}

}  // namespace

// static
AppShortcutLauncherItemController* AppShortcutLauncherItemController::Create(
    const std::string& app_id,
    const std::string& launch_id,
    ChromeLauncherController* controller) {
  if (app_id == ArcSupportHost::kHostAppId || app_id == arc::kPlayStoreAppId)
    return new ArcPlaystoreShortcutLauncherItemController(controller);
  return new AppShortcutLauncherItemController(app_id, launch_id, controller);
}

// Item controller for an app shortcut. Shortcuts track app and launcher ids,
// but do not have any associated windows (opening a shortcut will replace the
// item with the appropriate LauncherItemController type).
AppShortcutLauncherItemController::AppShortcutLauncherItemController(
    const std::string& app_id,
    const std::string& launch_id,
    ChromeLauncherController* controller)
    : LauncherItemController(app_id, launch_id, controller),
      chrome_launcher_controller_(controller) {
  // To detect V1 applications we use their domain and match them against the
  // used URL. This will also work with applications like Google Drive.
  const Extension* extension =
      GetExtensionForAppID(app_id, controller->profile());
  // Some unit tests have no real extension.
  if (extension) {
    set_refocus_url(GURL(
        extensions::AppLaunchInfo::GetLaunchWebURL(extension).spec() + "*"));
  }
}

AppShortcutLauncherItemController::~AppShortcutLauncherItemController() {}

ash::ShelfAction AppShortcutLauncherItemController::ItemSelected(
    ui::EventType event_type,
    int event_flags,
    int64_t display_id,
    ash::ShelfLaunchSource source) {
  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event_type == ui::ET_KEY_RELEASED && AdvanceToNextApp())
    return ash::SHELF_ACTION_WINDOW_ACTIVATED;

  content::WebContents* content = GetLRUApplication();
  if (!content) {
    // Ideally we come here only once. After that ShellLauncherItemController
    // will take over when the shell window gets opened. However there are apps
    // which take a lot of time for pre-processing (like the files app) before
    // they open a window. Since there is currently no other way to detect if an
    // app was started we suppress any further clicks within a special time out.
    if (IsV2App() && !AllowNextLaunchAttempt())
      return ash::SHELF_ACTION_NONE;

    // Launching some items replaces this item controller instance, which
    // destroys the app and launch id strings; making copies avoid crashes.
    launcher_controller()->LaunchApp(ash::AppLauncherId(app_id(), launch_id()),
                                     source, ui::EF_NONE);
    return ash::SHELF_ACTION_NEW_WINDOW_CREATED;
  }
  return ActivateContent(content);
}

ash::ShelfAppMenuItemList AppShortcutLauncherItemController::GetAppMenuItems(
    int event_flags) {
  ash::ShelfAppMenuItemList items;
  app_menu_items_ = GetRunningApplications();
  for (size_t i = 0; i < app_menu_items_.size(); i++) {
    content::WebContents* web_contents = app_menu_items_[i];
    gfx::Image icon = launcher_controller()->GetAppListIcon(web_contents);
    base::string16 title = launcher_controller()->GetAppListTitle(web_contents);
    items.push_back(base::MakeUnique<ash::ShelfApplicationMenuItem>(
        base::checked_cast<uint32_t>(i), title, &icon));
  }
  return items;
}

void AppShortcutLauncherItemController::ExecuteCommand(uint32_t command_id,
                                                       int event_flags) {
  if (static_cast<size_t>(command_id) >= app_menu_items_.size()) {
    app_menu_items_.clear();
    return;
  }

  // If the web contents was destroyed while the menu was open, then the invalid
  // pointer cached in |app_menu_items_| should yield a null browser or kNoTab.
  content::WebContents* web_contents = app_menu_items_[command_id];
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  TabStripModel* tab_strip = browser ? browser->tab_strip_model() : nullptr;
  const int index = tab_strip ? tab_strip->GetIndexOfWebContents(web_contents)
                              : TabStripModel::kNoTab;
  if (index != TabStripModel::kNoTab) {
    if (event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON)) {
      tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_USER_GESTURE);
    } else {
      multi_user_util::MoveWindowToCurrentDesktop(
          browser->window()->GetNativeWindow());
      tab_strip->ActivateTabAt(index, false);
      browser->window()->Show();
      browser->window()->Activate();
    }
  }

  app_menu_items_.clear();
}

void AppShortcutLauncherItemController::Close() {
  // Close all running 'programs' of this type.
  std::vector<content::WebContents*> content =
      launcher_controller()->GetV1ApplicationsFromAppId(app_id());
  for (size_t i = 0; i < content.size(); i++) {
    Browser* browser = chrome::FindBrowserWithWebContents(content[i]);
    if (!browser || !IsBrowserFromActiveUser(browser))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(content[i]);
    DCHECK(index != TabStripModel::kNoTab);
    tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_NONE);
  }
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
      GetExtensionForAppID(app_id(), launcher_controller()->profile());

  // It is possible to come here While an extension gets loaded.
  if (!extension)
    return items;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!IsBrowserFromActiveUser(browser))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(index);
      if (WebContentMatchesApp(
              extension, refocus_pattern, web_contents, browser))
        items.push_back(web_contents);
    }
  }
  return items;
}

content::WebContents* AppShortcutLauncherItemController::GetLRUApplication() {
  URLPattern refocus_pattern(URLPattern::SCHEME_ALL);
  refocus_pattern.SetMatchAllURLs(true);

  if (!refocus_url_.is_empty()) {
    refocus_pattern.SetMatchAllURLs(false);
    refocus_pattern.Parse(refocus_url_.spec());
  }

  const Extension* extension =
      GetExtensionForAppID(app_id(), launcher_controller()->profile());

  // We may get here while the extension is loading (and NULL).
  if (!extension)
    return NULL;

  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
    if (!CanBrowserBeUsedForDirectActivation(browser, launcher_controller()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    // We start to enumerate from the active index.
    int active_index = tab_strip->active_index();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(
          (index + active_index) % tab_strip->count());
      if (WebContentMatchesApp(
              extension, refocus_pattern, web_contents, browser))
        return web_contents;
    }
  }
  // Coming here our application was not in the LRU list. This could have
  // happened because it did never get activated yet. So check the browser list
  // as well.
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end(); ++it) {
    Browser* browser = *it;
    if (!CanBrowserBeUsedForDirectActivation(browser, launcher_controller()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(index);
      if (WebContentMatchesApp(
              extension, refocus_pattern, web_contents, browser))
        return web_contents;
    }
  }
  return NULL;
}

bool AppShortcutLauncherItemController::WebContentMatchesApp(
    const extensions::Extension* extension,
    const URLPattern& refocus_pattern,
    content::WebContents* web_contents,
    Browser* browser) {
  // If the browser is an app window, and the app name matches the extension,
  // then the contents match the app.
  if (browser->is_app()) {
    const extensions::Extension* browser_extension =
        ExtensionRegistry::Get(browser->profile())->GetExtensionById(
            web_app::GetExtensionIdFromApplicationName(browser->app_name()),
            ExtensionRegistry::EVERYTHING);
    return browser_extension == extension;
  }

  // Apps set to launch in app windows should not match contents running in
  // tabs.
  if (extensions::LaunchesInWindow(browser->profile(), extension))
    return false;

  // There are three ways to identify the association of a URL with this
  // extension:
  // - The refocus pattern is matched (needed for apps like drive).
  // - The extension's origin + extent gets matched.
  // - The launcher controller knows that the tab got created for this app.
  const GURL tab_url = web_contents->GetURL();
  return ((!refocus_pattern.match_all_urls() &&
           refocus_pattern.MatchesURL(tab_url)) ||
          (extension->OverlapsWithOrigin(tab_url) &&
           extension->web_extent().MatchesURL(tab_url)) ||
          launcher_controller()->IsWebContentHandledByApplication(web_contents,
                                                                  app_id()));
}

ash::ShelfAction AppShortcutLauncherItemController::ActivateContent(
    content::WebContents* content) {
  Browser* browser = chrome::FindBrowserWithWebContents(content);
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(content);
  DCHECK_NE(TabStripModel::kNoTab, index);

  int old_index = tab_strip->active_index();
  if (index != old_index)
    tab_strip->ActivateTabAt(index, false);
  return launcher_controller()->ActivateWindowOrMinimizeIfActive(
      browser->window(),
      index == old_index && GetRunningApplications().size() == 1);
}

bool AppShortcutLauncherItemController::AdvanceToNextApp() {
  std::vector<content::WebContents*> items = GetRunningApplications();
  if (items.size() >= 1) {
    Browser* browser = chrome::FindBrowserWithWindow(
        ash::wm::GetActiveWindow());
    if (browser) {
      TabStripModel* tab_strip = browser->tab_strip_model();
      content::WebContents* active = tab_strip->GetWebContentsAt(
          tab_strip->active_index());
      std::vector<content::WebContents*>::const_iterator i(
          std::find(items.begin(), items.end(), active));
      if (i != items.end()) {
        if (items.size() == 1) {
          // If there is only a single item available, we animate it upon key
          // action.
          AnimateWindow(browser->window()->GetNativeWindow(),
              wm::WINDOW_ANIMATION_TYPE_BOUNCE);
        } else {
          int index = (static_cast<int>(i - items.begin()) + 1) % items.size();
          ActivateContent(items[index]);
        }
        return true;
      }
    }
  }
  return false;
}

bool AppShortcutLauncherItemController::IsV2App() {
  const Extension* extension =
      GetExtensionForAppID(app_id(), launcher_controller()->profile());
  return extension && extension->is_platform_app();
}

bool AppShortcutLauncherItemController::AllowNextLaunchAttempt() {
  if (last_launch_attempt_.is_null() ||
      last_launch_attempt_ + base::TimeDelta::FromMilliseconds(
          kClickSuppressionInMS) < base::Time::Now()) {
    last_launch_attempt_ = base::Time::Now();
    return true;
  }
  return false;
}
