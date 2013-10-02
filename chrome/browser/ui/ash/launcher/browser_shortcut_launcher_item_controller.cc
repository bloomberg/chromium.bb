// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"

#include <vector>

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_browser.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_tab.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/ash_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image.h"
#include "ui/views/corewm/window_animations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"
#endif

BrowserShortcutLauncherItemController::BrowserShortcutLauncherItemController(
    ChromeLauncherController* launcher_controller)
    : LauncherItemController(TYPE_SHORTCUT,
                             extension_misc::kChromeAppId,
                             launcher_controller) {
}

BrowserShortcutLauncherItemController::
    ~BrowserShortcutLauncherItemController() {
}

void BrowserShortcutLauncherItemController::UpdateBrowserItemState() {
  // The shell will not be available for win7_aura unittests like
  // ChromeLauncherControllerTest.BrowserMenuGeneration.
  if (!ash::Shell::HasInstance())
    return;

  ash::LauncherModel* model = launcher_controller()->model();

  // Determine the new browser's active state and change if necessary.
  size_t browser_index = ash::GetBrowserItemIndex(*model);
  DCHECK_GE(browser_index, 0u);
  ash::LauncherItem browser_item = model->items()[browser_index];
  ash::LauncherItemStatus browser_status = ash::STATUS_CLOSED;

  aura::Window* window = ash::wm::GetActiveWindow();
  if (window) {
    // Check if the active browser / tab is a browser which is not an app,
    // a windowed app, a popup or any other item which is not a browser of
    // interest.
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (IsBrowserRepresentedInBrowserList(browser)) {
      browser_status = ash::STATUS_ACTIVE;
      // If an app is running in active window, browser item status cannot be
      // active.
      if (launcher_controller()->GetIDByWindow(window) != browser_item.id)
        browser_status = ash::STATUS_RUNNING;
    }
  }

  if (browser_status == ash::STATUS_CLOSED) {
    const BrowserList* ash_browser_list =
        BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
    for (BrowserList::const_reverse_iterator it =
             ash_browser_list->begin_last_active();
         it != ash_browser_list->end_last_active() &&
         browser_status == ash::STATUS_CLOSED; ++it) {
      if (IsBrowserRepresentedInBrowserList(*it))
        browser_status = ash::STATUS_RUNNING;
    }
  }

  if (browser_status != browser_item.status) {
    browser_item.status = browser_status;
    model->Set(browser_index, browser_item);
  }
}

bool BrowserShortcutLauncherItemController::IsCurrentlyShownInWindow(
    aura::Window* window) const {
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_iterator it = ash_browser_list->begin();
       it != ash_browser_list->end(); ++it) {
    // During browser creation it is possible that this function is called
    // before a browser got a window (see crbug.com/263563).
    if ((*it)->window() &&
        (*it)->window()->GetNativeWindow() == window)
      return true;
  }
  return false;
}

bool BrowserShortcutLauncherItemController::IsOpen() const {
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  return ash_browser_list->empty() ? false : true;
}

bool BrowserShortcutLauncherItemController::IsVisible() const {
  Browser* last_browser = chrome::FindTabbedBrowser(
      launcher_controller()->profile(),
      true,
      chrome::HOST_DESKTOP_TYPE_ASH);

  if (!last_browser) {
    return false;
  }

  aura::Window* window = last_browser->window()->GetNativeWindow();
  return ash::wm::IsActiveWindow(window);
}

void BrowserShortcutLauncherItemController::Launch(ash::LaunchSource source,
                                                   int event_flags) {
}

void BrowserShortcutLauncherItemController::Activate(ash::LaunchSource source) {
  Browser* last_browser = chrome::FindTabbedBrowser(
      launcher_controller()->profile(),
      true,
      chrome::HOST_DESKTOP_TYPE_ASH);

  if (!last_browser) {
    launcher_controller()->CreateNewWindow();
    return;
  }

  launcher_controller()->ActivateWindowOrMinimizeIfActive(
      last_browser->window(), GetApplicationList(0).size() == 2);
}

void BrowserShortcutLauncherItemController::Close() {
}

ChromeLauncherAppMenuItems
BrowserShortcutLauncherItemController::GetApplicationList(int event_flags) {
  ChromeLauncherAppMenuItems items;
  bool found_tabbed_browser = false;
  // Add the application name to the menu.
  items.push_back(new ChromeLauncherAppMenuItem(GetTitle(), NULL, false));
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_iterator it = ash_browser_list->begin();
       it != ash_browser_list->end(); ++it) {
    Browser* browser = *it;
    // Make sure that the browser was already shown and it has a proper window.
    if (std::find(ash_browser_list->begin_last_active(),
                  ash_browser_list->end_last_active(),
                  browser) == ash_browser_list->end_last_active() ||
        !browser->window())
      continue;
    if (browser->is_type_tabbed())
      found_tabbed_browser = true;
    else if (!IsBrowserRepresentedInBrowserList(browser))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (tab_strip->active_index() == -1)
      continue;
    if (!(event_flags & ui::EF_SHIFT_DOWN)) {
      content::WebContents* web_contents =
          tab_strip->GetWebContentsAt(tab_strip->active_index());
      gfx::Image app_icon = GetBrowserListIcon(web_contents);
      string16 title = GetBrowserListTitle(web_contents);
      items.push_back(new ChromeLauncherAppMenuItemBrowser(
          title, &app_icon, browser, items.size() == 1));
    } else {
      for (int index = 0; index  < tab_strip->count(); ++index) {
        content::WebContents* web_contents =
            tab_strip->GetWebContentsAt(index);
        gfx::Image app_icon =
            launcher_controller()->GetAppListIcon(web_contents);
        string16 title = launcher_controller()->GetAppListTitle(web_contents);
        // Check if we need to insert a separator in front.
        bool leading_separator = !index;
        items.push_back(new ChromeLauncherAppMenuItemTab(
            title, &app_icon, web_contents, leading_separator));
      }
    }
  }
  // If only windowed applications are open, we return an empty list to
  // enforce the creation of a new browser.
  if (!found_tabbed_browser)
    items.clear();
  return items.Pass();
}

void BrowserShortcutLauncherItemController::ItemSelected(
    const ui::Event& event) {
#if defined(OS_CHROMEOS)
  chromeos::default_pinned_apps_field_trial::RecordShelfClick(
      chromeos::default_pinned_apps_field_trial::CHROME);
#endif

  if (event.flags() & ui::EF_CONTROL_DOWN) {
    launcher_controller()->CreateNewWindow();
    return;
  }

  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event.type() & ui::ET_KEY_RELEASED) {
    ActivateOrAdvanceToNextBrowser();
    return;
  }

  Activate(ash::LAUNCH_FROM_UNKNOWN);
}

string16 BrowserShortcutLauncherItemController::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

ui::MenuModel* BrowserShortcutLauncherItemController::CreateContextMenu(
    aura::RootWindow* root_window) {
  ash::LauncherItem item =
      *(launcher_controller()->model()->ItemByID(launcher_id()));
  return new LauncherContextMenu(launcher_controller(), &item, root_window);
}

ash::LauncherMenuModel*
BrowserShortcutLauncherItemController::CreateApplicationMenu(int event_flags) {
  return new LauncherApplicationMenuItemModel(GetApplicationList(event_flags));
}

bool BrowserShortcutLauncherItemController::IsDraggable() {
  return launcher_controller()->CanPin() ? true : false;
}

bool BrowserShortcutLauncherItemController::ShouldShowTooltip() {
  return true;
}

gfx::Image BrowserShortcutLauncherItemController::GetBrowserListIcon(
    content::WebContents* web_contents) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetImageNamed(IsIncognito(web_contents) ?
      IDR_AURA_LAUNCHER_LIST_INCOGNITO_BROWSER :
      IDR_AURA_LAUNCHER_LIST_BROWSER);
}

string16 BrowserShortcutLauncherItemController::GetBrowserListTitle(
    content::WebContents* web_contents) const {
  string16 title = web_contents->GetTitle();
  if (!title.empty())
    return title;
  return l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
}

bool BrowserShortcutLauncherItemController::IsIncognito(
    content::WebContents* web_contents) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return profile->IsOffTheRecord() && !profile->IsGuestSession();
}

void BrowserShortcutLauncherItemController::ActivateOrAdvanceToNextBrowser() {
  // Create a list of all suitable running browsers.
  std::vector<Browser*> items;
  // We use the list in the order of how the browsers got created - not the LRU
  // order.
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_iterator it =
           ash_browser_list->begin();
       it != ash_browser_list->end(); ++it) {
    if (IsBrowserRepresentedInBrowserList(*it))
      items.push_back(*it);
  }
  // If there are no suitable browsers we create a new one.
  if (items.empty()) {
    launcher_controller()->CreateNewWindow();
    return;
  }
  Browser* browser = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  if (items.size() == 1) {
    // If there is only one suitable browser, we can either activate it, or
    // bounce it (if it is already active).
    if (browser == items[0]) {
      AnimateWindow(browser->window()->GetNativeWindow(),
                    views::corewm::WINDOW_ANIMATION_TYPE_BOUNCE);
      return;
    }
    browser = items[0];
  } else {
    // If there is more then one suitable browser, we advance to the next if
    // |browser| is already active - or - check the last used browser if it can
    // be used.
    std::vector<Browser*>::iterator i =
        std::find(items.begin(), items.end(), browser);
    if (i != items.end()) {
      browser = (++i == items.end()) ? items[0] : *i;
    } else {
      browser = chrome::FindTabbedBrowser(launcher_controller()->profile(),
                                          true,
                                          chrome::HOST_DESKTOP_TYPE_ASH);
      if (!browser ||
          !IsBrowserRepresentedInBrowserList(browser))
        browser = items[0];
    }
  }
  DCHECK(browser);
  browser->window()->Show();
  browser->window()->Activate();
}

bool BrowserShortcutLauncherItemController::IsBrowserRepresentedInBrowserList(
    Browser* browser) {
  return (browser &&
          browser->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH &&
          (browser->is_type_tabbed() ||
           !browser->is_app() ||
           !browser->is_type_popup() ||
           launcher_controller()->
               GetLauncherIDForAppID(web_app::GetExtensionIdFromApplicationName(
                   browser->app_name())) <= 0));
}
