// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/renderer_preferences.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionPrefs;

namespace {

// Get the launch URL for a given extension, with optional override/fallback.
// |override_url|, if non-empty, will be preferred over the extension's
// launch url.
GURL UrlForExtension(const Extension* extension,
                     const GURL& override_url) {
  if (!extension)
    return override_url;

  GURL url;
  if (!override_url.is_empty()) {
    DCHECK(extension->web_extent().MatchesURL(override_url) ||
           override_url.GetOrigin() == extension->url());
    url = override_url;
  } else {
    url = extension->GetFullLaunchURL();
  }

  // For extensions lacking launch urls, determine a reasonable fallback.
  if (!url.is_valid()) {
    url = extensions::ManifestURL::GetOptionsPage(extension);
    if (!url.is_valid())
      url = GURL(chrome::kChromeUIExtensionsURL);
  }

  return url;
}

WebContents* OpenApplicationWindow(
    Profile* profile,
    const Extension* extension,
    extension_misc::LaunchContainer container,
    const GURL& url_input,
    Browser** app_browser,
    const gfx::Rect& override_bounds) {
  DCHECK(!url_input.is_empty() || extension);
  GURL url = UrlForExtension(extension, url_input);

  std::string app_name;
  app_name = extension ?
      web_app::GenerateApplicationNameFromExtensionId(extension->id()) :
      web_app::GenerateApplicationNameFromURL(url);

  Browser::Type type = Browser::TYPE_POPUP;

  gfx::Rect window_bounds;
  if (extension) {
    window_bounds.set_width(extension->launch_width());
    window_bounds.set_height(extension->launch_height());
  } else if (!override_bounds.IsEmpty()) {
    window_bounds = override_bounds;
  }

  Browser::CreateParams params(type, profile, chrome::GetActiveDesktop());
  params.app_name = app_name;
  params.initial_bounds = window_bounds;

#if defined(USE_ASH)
  if (extension &&
      container == extension_misc::LAUNCH_WINDOW) {
    // In ash, LAUNCH_FULLSCREEN launches in a maximized app window and
    // LAUNCH_WINDOW launches in a normal app window.
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    ExtensionPrefs::LaunchType launch_type =
        service->extension_prefs()->GetLaunchType(
            extension, ExtensionPrefs::LAUNCH_DEFAULT);
    if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN)
      params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
    else if (launch_type == ExtensionPrefs::LAUNCH_WINDOW)
      params.initial_show_state = ui::SHOW_STATE_NORMAL;
  }
#endif

  Browser* browser = NULL;
#if defined(OS_WIN)
  // On Windows 8's single window Metro mode we don't allow multiple Chrome
  // windows to be created. We instead attempt to reuse an existing Browser
  // window.
  if (win8::IsSingleWindowMetroMode()) {
    browser = chrome::FindBrowserWithProfile(
        profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
  }
#endif
  if (!browser)
    browser = new Browser(params);

  if (app_browser)
    *app_browser = browser;

  WebContents* web_contents = chrome::AddSelectedTabWithURL(
      browser, url, content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();

  browser->window()->Show();

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  web_contents->GetView()->SetInitialFocus();
  return web_contents;
}

WebContents* OpenApplicationTab(Profile* profile,
                                const Extension* extension,
                                const GURL& override_url,
                                WindowOpenDisposition disposition) {
  Browser* browser = chrome::FindTabbedBrowser(profile,
                                               false,
                                               chrome::GetActiveDesktop());
  WebContents* contents = NULL;
  if (!browser) {
    // No browser for this profile, need to open a new one.
    browser = new Browser(Browser::CreateParams(Browser::TYPE_TABBED,
                                                profile,
                                                chrome::GetActiveDesktop()));
    browser->window()->Show();
    // There's no current tab in this browser window, so add a new one.
    disposition = NEW_FOREGROUND_TAB;
  } else {
    // For existing browser, ensure its window is shown and activated.
    browser->window()->Show();
    browser->window()->Activate();
  }

  // Check the prefs for overridden mode.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extension_service);

  ExtensionPrefs::LaunchType launch_type =
      extension_service->extension_prefs()->GetLaunchType(
          extension, ExtensionPrefs::LAUNCH_DEFAULT);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);

  int add_type = TabStripModel::ADD_ACTIVE;
  if (launch_type == ExtensionPrefs::LAUNCH_PINNED)
    add_type |= TabStripModel::ADD_PINNED;

  GURL extension_url = UrlForExtension(extension, override_url);
  // TODO(erikkay): START_PAGE doesn't seem like the right transition in all
  // cases.
  chrome::NavigateParams params(browser, extension_url,
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.tabstrip_add_types = add_type;
  params.disposition = disposition;

  if (disposition == CURRENT_TAB) {
    WebContents* existing_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    TabStripModel* model = browser->tab_strip_model();
    int tab_index = model->GetIndexOfWebContents(existing_tab);

    existing_tab->OpenURL(content::OpenURLParams(
          extension_url,
          content::Referrer(existing_tab->GetURL(),
                            WebKit::WebReferrerPolicyDefault),
          disposition, content::PAGE_TRANSITION_LINK, false));
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
    DCHECK(launch_type != ExtensionPrefs::LAUNCH_FULLSCREEN);
  } else {
    // TODO(skerner):  If we are already in full screen mode, and the user
    // set the app to open as a regular or pinned tab, what should happen?
    // Today we open the tab, but stay in full screen mode.  Should we leave
    // full screen mode in this case?
    if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN &&
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

}  // namespace

namespace chrome {

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 extension_misc::LaunchContainer container,
                                 WindowOpenDisposition disposition)
    : profile(profile),
      extension(extension),
      container(container),
      disposition(disposition),
      override_url(),
      command_line(NULL) {}

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 WindowOpenDisposition disposition)
    : profile(profile),
      extension(extension),
      container(extension_misc::LAUNCH_NONE),
      disposition(disposition),
      override_url(),
      command_line(NULL) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);

  // Look up the app preference to find out the right launch container. Default
  // is to launch as a regular tab.
  container = service->extension_prefs()->GetLaunchContainer(
      extension, extensions::ExtensionPrefs::LAUNCH_REGULAR);
}

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 int event_flags)
    : profile(profile),
      extension(extension),
      container(extension_misc::LAUNCH_NONE),
      disposition(ui::DispositionFromEventFlags(event_flags)),
      override_url(),
      command_line(NULL) {
  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB) {
    container = extension_misc::LAUNCH_TAB;
  } else if (disposition == NEW_WINDOW) {
    container = extension_misc::LAUNCH_WINDOW;
  } else {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    DCHECK(service);

    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    container = service->extension_prefs()->GetLaunchContainer(
        extension, extensions::ExtensionPrefs::LAUNCH_DEFAULT);
    disposition = NEW_FOREGROUND_TAB;
  }
}

WebContents* OpenApplication(const AppLaunchParams& params) {
  Profile* profile = params.profile;
  const extensions::Extension* extension = params.extension;
  extension_misc::LaunchContainer container = params.container;
  const GURL& override_url = params.override_url;

  WebContents* tab = NULL;
  ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(profile)->
      extension_service()->extension_prefs();
  prefs->SetActiveBit(extension->id(), true);

  UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunchContainer", container, 100);

  if (extension->is_platform_app()) {
    extensions::LaunchPlatformApp(profile, extension, params.command_line,
                                  params.current_directory);
    return NULL;
  }

  switch (container) {
    case extension_misc::LAUNCH_NONE: {
      NOTREACHED();
      break;
    }
    case extension_misc::LAUNCH_PANEL:
    case extension_misc::LAUNCH_WINDOW:
      tab = OpenApplicationWindow(profile, extension, container,
                                  override_url, NULL, gfx::Rect());
      break;
    case extension_misc::LAUNCH_TAB: {
      tab = OpenApplicationTab(profile, extension, override_url,
                               params.disposition);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return tab;
}

WebContents* OpenAppShortcutWindow(Profile* profile,
                                   const GURL& url,
                                   const gfx::Rect& override_bounds) {
  Browser* app_browser;
  WebContents* tab = OpenApplicationWindow(
      profile,
      NULL,  // this is a URL app.  No extension.
      extension_misc::LAUNCH_WINDOW,
      url,
      &app_browser,
      override_bounds);

  if (!tab)
    return NULL;

  // Set UPDATE_SHORTCUT as the pending web app action. This action is picked
  // up in LoadingStateChanged to schedule a GetApplicationInfo. And when
  // the web app info is available, extensions::TabHelper notifies Browser via
  // OnDidGetApplicationInfo, which calls
  // web_app::UpdateShortcutForTabContents when it sees UPDATE_SHORTCUT as
  // pending web app action.
  extensions::TabHelper::FromWebContents(tab)->set_pending_web_app_action(
      extensions::TabHelper::UPDATE_SHORTCUT);

  return tab;
}

}  // namespace chrome
