// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch_web_app.h"

#include <string>

#include "apps/launcher.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace {

const Extension* GetExtension(const AppLaunchParams& params) {
  if (params.extension_id.empty())
    return NULL;
  ExtensionRegistry* registry = ExtensionRegistry::Get(params.profile);
  return registry->GetExtensionById(params.extension_id,
                                    ExtensionRegistry::ENABLED |
                                        ExtensionRegistry::DISABLED |
                                        ExtensionRegistry::TERMINATED);
}

ui::WindowShowState DetermineWindowShowState(
    Profile* profile,
    extensions::LaunchContainer container,
    const Extension* extension) {
  if (!extension || container != extensions::LAUNCH_CONTAINER_WINDOW)
    return ui::SHOW_STATE_DEFAULT;

  if (chrome::IsRunningInForcedAppMode())
    return ui::SHOW_STATE_FULLSCREEN;

#if defined(USE_ASH)
  // In ash, LAUNCH_TYPE_FULLSCREEN launches in a maximized app window and
  // LAUNCH_TYPE_WINDOW launches in a normal app window.
  extensions::LaunchType launch_type =
      extensions::GetLaunchType(ExtensionPrefs::Get(profile), extension);
  if (launch_type == extensions::LAUNCH_TYPE_FULLSCREEN)
    return ui::SHOW_STATE_MAXIMIZED;
  else if (launch_type == extensions::LAUNCH_TYPE_WINDOW)
    return ui::SHOW_STATE_NORMAL;
#endif

  return ui::SHOW_STATE_DEFAULT;
}

}  // namespace

WebContents* OpenWebAppWindow(const AppLaunchParams& params, const GURL& url) {
  Profile* const profile = params.profile;
  const Extension* const extension = GetExtension(params);

  std::string app_name = extension ?
      web_app::GenerateApplicationNameFromExtensionId(extension->id()) :
      web_app::GenerateApplicationNameFromURL(url);

  gfx::Rect initial_bounds;
  if (!params.override_bounds.IsEmpty()) {
    initial_bounds = params.override_bounds;
  } else if (extension) {
    initial_bounds.set_width(
        extensions::AppLaunchInfo::GetLaunchWidth(extension));
    initial_bounds.set_height(
        extensions::AppLaunchInfo::GetLaunchHeight(extension));
  }

  Browser::CreateParams browser_params(
      Browser::CreateParams::CreateForApp(app_name,
                                          true /* trusted_source */,
                                          initial_bounds,
                                          profile,
                                          params.desktop_type));

  browser_params.initial_show_state = DetermineWindowShowState(profile,
                                                               params.container,
                                                               extension);

  Browser* browser = new Browser(browser_params);

  WebContents* web_contents = chrome::AddSelectedTabWithURL(
      browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();

  browser->window()->Show();

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  web_contents->SetInitialFocus();
  return web_contents;
}

WebContents* OpenWebAppTab(const AppLaunchParams& launch_params,
                           const GURL& url) {
  const Extension* extension = GetExtension(launch_params);
  CHECK(extension);
  Profile* const profile = launch_params.profile;
  WindowOpenDisposition disposition = launch_params.disposition;

  Browser* browser = chrome::FindTabbedBrowser(profile,
                                               false,
                                               launch_params.desktop_type);
  WebContents* contents = NULL;
  if (!browser) {
    // No browser for this profile, need to open a new one.
    browser = new Browser(Browser::CreateParams(Browser::TYPE_TABBED,
                                                profile,
                                                launch_params.desktop_type));
    browser->window()->Show();
    // There's no current tab in this browser window, so add a new one.
    disposition = NEW_FOREGROUND_TAB;
  } else {
    // For existing browser, ensure its window is shown and activated.
    browser->window()->Show();
    browser->window()->Activate();
  }

  extensions::LaunchType launch_type =
      extensions::GetLaunchType(ExtensionPrefs::Get(profile), extension);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);

  int add_type = TabStripModel::ADD_ACTIVE;
  if (launch_type == extensions::LAUNCH_TYPE_PINNED)
    add_type |= TabStripModel::ADD_PINNED;

  chrome::NavigateParams params(browser, url,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.tabstrip_add_types = add_type;
  params.disposition = disposition;

  if (disposition == CURRENT_TAB) {
    WebContents* existing_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    TabStripModel* model = browser->tab_strip_model();
    int tab_index = model->GetIndexOfWebContents(existing_tab);

    existing_tab->OpenURL(content::OpenURLParams(
          url,
          content::Referrer(existing_tab->GetURL(),
                            blink::WebReferrerPolicyDefault),
          disposition, ui::PAGE_TRANSITION_LINK, false));
    // Reset existing_tab as OpenURL() may have clobbered it.
    existing_tab = browser->tab_strip_model()->GetActiveWebContents();
    if (params.tabstrip_add_types & TabStripModel::ADD_PINNED) {
      model->SetTabPinned(tab_index, true);
      // Pinning may have moved the tab.
      tab_index = model->GetIndexOfWebContents(existing_tab);
    }
    if (params.tabstrip_add_types & TabStripModel::ADD_ACTIVE)
      model->ActivateTabAt(tab_index, true);

    contents = existing_tab;
  } else {
    chrome::Navigate(&params);
    contents = params.target_contents;
  }

  // On Chrome OS the host desktop type for a browser window is always set to
  // HOST_DESKTOP_TYPE_ASH. On Windows 8 it is only the case for Chrome ASH
  // in metro mode.
  if (browser->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH) {
    // In ash, LAUNCH_FULLSCREEN launches in the OpenApplicationWindow function
    // i.e. it should not reach here.
    DCHECK(launch_type != extensions::LAUNCH_TYPE_FULLSCREEN);
  } else {
    // TODO(skerner):  If we are already in full screen mode, and the user
    // set the app to open as a regular or pinned tab, what should happen?
    // Today we open the tab, but stay in full screen mode.  Should we leave
    // full screen mode in this case?
    if (launch_type == extensions::LAUNCH_TYPE_FULLSCREEN &&
        !browser->window()->IsFullscreen()) {
#if defined(OS_MACOSX)
      chrome::ToggleFullscreenWithChromeOrFallback(browser);
#else
      chrome::ToggleFullscreenMode(browser);
#endif
    }
  }
  return contents;
}
