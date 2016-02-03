// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch.h"

#include <string>

#include "apps/launcher.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/per_app_settings_service.h"
#include "chrome/browser/apps/per_app_settings_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace {

// Shows the app list for |desktop_type| and returns the app list's window.
gfx::NativeWindow ShowAppListAndGetNativeWindow(
      chrome::HostDesktopType desktop_type) {
  AppListService* app_list_service = AppListService::Get(desktop_type);
  app_list_service->Show();
  return app_list_service->GetAppListWindow();
}

// Attempts to launch an app, prompting the user to enable it if necessary. If
// a prompt is required it will be shown inside the window returned by
// |parent_window_getter|.
// This class manages its own lifetime.
class EnableViaDialogFlow : public ExtensionEnableFlowDelegate {
 public:
  EnableViaDialogFlow(
      ExtensionService* service,
      Profile* profile,
      const std::string& extension_id,
      const base::Callback<gfx::NativeWindow(void)>& parent_window_getter,
      const base::Closure& callback)
      : service_(service),
        profile_(profile),
        extension_id_(extension_id),
        parent_window_getter_(parent_window_getter),
        callback_(callback) {
  }

  ~EnableViaDialogFlow() override {}

  void Run() {
    DCHECK(!service_->IsExtensionEnabled(extension_id_));
    flow_.reset(new ExtensionEnableFlow(profile_, extension_id_, this));
    flow_->StartForCurrentlyNonexistentWindow(parent_window_getter_);
  }

 private:
  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override {
    const Extension* extension =
        service_->GetExtensionById(extension_id_, false);
    if (!extension)
      return;
    callback_.Run();
    delete this;
  }

  void ExtensionEnableFlowAborted(bool user_initiated) override { delete this; }

  ExtensionService* service_;
  Profile* profile_;
  std::string extension_id_;
  base::Callback<gfx::NativeWindow(void)> parent_window_getter_;
  base::Closure callback_;
  scoped_ptr<ExtensionEnableFlow> flow_;

  DISALLOW_COPY_AND_ASSIGN(EnableViaDialogFlow);
};

const Extension* GetExtension(const AppLaunchParams& params) {
  if (params.extension_id.empty())
    return NULL;
  ExtensionRegistry* registry = ExtensionRegistry::Get(params.profile);
  return registry->GetExtensionById(params.extension_id,
                                    ExtensionRegistry::ENABLED |
                                        ExtensionRegistry::DISABLED |
                                        ExtensionRegistry::TERMINATED);
}

// Get the launch URL for a given extension, with optional override/fallback.
// |override_url|, if non-empty, will be preferred over the extension's
// launch url.
GURL UrlForExtension(const extensions::Extension* extension,
                     const GURL& override_url) {
  if (!extension)
    return override_url;

  GURL url;
  if (!override_url.is_empty()) {
    DCHECK(extension->web_extent().MatchesURL(override_url) ||
           override_url.GetOrigin() == extension->url());
    url = override_url;
  } else {
    url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
  }

  // For extensions lacking launch urls, determine a reasonable fallback.
  if (!url.is_valid()) {
    url = extensions::OptionsPageInfo::GetOptionsPage(extension);
    if (!url.is_valid())
      url = GURL(chrome::kChromeUIExtensionsURL);
  }

  return url;
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
  // LAUNCH_TYPE_WINDOW launches in a default app window.
  extensions::LaunchType launch_type =
      extensions::GetLaunchType(ExtensionPrefs::Get(profile), extension);
  if (launch_type == extensions::LAUNCH_TYPE_FULLSCREEN)
    return ui::SHOW_STATE_MAXIMIZED;
  else if (launch_type == extensions::LAUNCH_TYPE_WINDOW)
    return ui::SHOW_STATE_DEFAULT;
#endif

  return ui::SHOW_STATE_DEFAULT;
}

WebContents* OpenApplicationWindow(const AppLaunchParams& params,
                                   const GURL& url) {
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
  ui::PageTransition transition =
      (extension ? ui::PAGE_TRANSITION_AUTO_BOOKMARK
                 : ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

  WebContents* web_contents =
      chrome::AddSelectedTabWithURL(browser, url, transition);
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();

  browser->window()->Show();

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  web_contents->SetInitialFocus();
  return web_contents;
}

WebContents* OpenApplicationTab(const AppLaunchParams& launch_params,
                           const GURL& url) {
  const Extension* extension = GetExtension(launch_params);
  CHECK(extension);
  Profile* const profile = launch_params.profile;
  WindowOpenDisposition disposition = launch_params.disposition;

  Browser* browser = chrome::FindTabbedBrowser(profile, false);
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

  ui::PageTransition transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  chrome::NavigateParams params(browser, url, transition);
  params.tabstrip_add_types = add_type;
  params.disposition = disposition;

  if (disposition == CURRENT_TAB) {
    WebContents* existing_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    TabStripModel* model = browser->tab_strip_model();
    int tab_index = model->GetIndexOfWebContents(existing_tab);

    existing_tab->OpenURL(content::OpenURLParams(
        url, content::Referrer::SanitizeForRequest(
                 url, content::Referrer(existing_tab->GetURL(),
                                        blink::WebReferrerPolicyDefault)),
        disposition, transition, false));
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
      chrome::ToggleFullscreenWithToolbarOrFallback(browser);
#else
      chrome::ToggleFullscreenMode(browser);
#endif
    }
  }
  return contents;
}

WebContents* OpenEnabledApplication(const AppLaunchParams& params) {
  const Extension* extension = GetExtension(params);
  if (!extension)
    return NULL;
  Profile* profile = params.profile;

  WebContents* tab = NULL;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  prefs->SetActiveBit(extension->id(), true);

  if (CanLaunchViaEvent(extension)) {
    // Remember what desktop the launch happened on so that when the app opens a
    // window we can open them on the right desktop.
    PerAppSettingsServiceFactory::GetForBrowserContext(profile)->
        SetDesktopLastLaunchedFrom(extension->id(), params.desktop_type);

    apps::LaunchPlatformAppWithCommandLine(profile,
                                           extension,
                                           params.command_line,
                                           params.current_directory,
                                           params.source);
    return NULL;
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.HostedAppLaunchContainer",
                            params.container,
                            extensions::NUM_LAUNCH_CONTAINERS);

  GURL url = UrlForExtension(extension, params.override_url);
  if (extension->from_bookmark()) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchContainer",
                              params.container,
                              extensions::NUM_LAUNCH_CONTAINERS);

    // Record the launch time in the site engagement service. A recent bookmark
    // app launch will provide an engagement boost to the origin.
    SiteEngagementService* service = SiteEngagementService::Get(profile);
    if (service)
      service->SetLastShortcutLaunchTime(url);
  }

  // Record v1 app launch. Platform app launch is recorded when dispatching
  // the onLaunched event.
  prefs->SetLastLaunchTime(extension->id(), base::Time::Now());

  switch (params.container) {
    case extensions::LAUNCH_CONTAINER_NONE: {
      NOTREACHED();
      break;
    }
    case extensions::LAUNCH_CONTAINER_PANEL:
    case extensions::LAUNCH_CONTAINER_WINDOW:
      tab = OpenApplicationWindow(params, url);
      break;
    case extensions::LAUNCH_CONTAINER_TAB: {
      tab = OpenApplicationTab(params, url);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return tab;
}

}  // namespace

WebContents* OpenApplication(const AppLaunchParams& params) {
  return OpenEnabledApplication(params);
}

void OpenApplicationWithReenablePrompt(const AppLaunchParams& params) {
  const Extension* extension = GetExtension(params);
  if (!extension)
    return;
  Profile* profile = params.profile;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service->IsExtensionEnabled(extension->id()) ||
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          extension->id(), extensions::ExtensionRegistry::TERMINATED)) {
  base::Callback<gfx::NativeWindow(void)> dialog_parent_window_getter;
  // TODO(pkotwicz): Figure out which window should be used as the parent for
  // the "enable application" dialog in Athena.
  dialog_parent_window_getter =
      base::Bind(&ShowAppListAndGetNativeWindow, params.desktop_type);
    (new EnableViaDialogFlow(
        service, profile, extension->id(), dialog_parent_window_getter,
        base::Bind(base::IgnoreResult(OpenEnabledApplication), params)))->Run();
    return;
  }

  OpenEnabledApplication(params);
}

WebContents* OpenAppShortcutWindow(Profile* profile,
                                   const GURL& url) {
  AppLaunchParams launch_params(profile,
                                NULL,  // this is a URL app.  No extension.
                                extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW,
                                extensions::SOURCE_COMMAND_LINE);
  launch_params.override_url = url;

  WebContents* tab = OpenApplicationWindow(launch_params, url);

  if (!tab)
    return NULL;

  extensions::TabHelper::FromWebContents(tab)->UpdateShortcutOnLoadComplete();

  return tab;
}

bool CanLaunchViaEvent(const extensions::Extension* extension) {
  const extensions::Feature* feature =
      extensions::FeatureProvider::GetAPIFeature("app.runtime");
  return feature->IsAvailableToExtension(extension).is_available();
}
