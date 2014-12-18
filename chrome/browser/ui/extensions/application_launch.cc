// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch.h"

#include <string>

#include "apps/launcher.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/apps/per_app_settings_service.h"
#include "chrome/browser/apps/per_app_settings_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch_web_app.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/options_page_info.h"

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace {

#if !defined(USE_ATHENA)
// Shows the app list for |desktop_type| and returns the app list's window.
gfx::NativeWindow ShowAppListAndGetNativeWindow(
      chrome::HostDesktopType desktop_type) {
  AppListService* app_list_service = AppListService::Get(desktop_type);
  app_list_service->Show();
  return app_list_service->GetAppListWindow();
}
#endif

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

  // Record v1 app launch. Platform app launch is recorded when dispatching
  // the onLaunched event.
  prefs->SetLastLaunchTime(extension->id(), base::Time::Now());

  GURL url = UrlForExtension(extension, params.override_url);
  switch (params.container) {
    case extensions::LAUNCH_CONTAINER_NONE: {
      NOTREACHED();
      break;
    }
    case extensions::LAUNCH_CONTAINER_PANEL:
    case extensions::LAUNCH_CONTAINER_WINDOW:
      tab = OpenWebAppWindow(params, url);
      break;
    case extensions::LAUNCH_CONTAINER_TAB: {
      tab = OpenWebAppTab(params, url);
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
#if !defined(USE_ATHENA)
  dialog_parent_window_getter =
      base::Bind(&ShowAppListAndGetNativeWindow, params.desktop_type);
#endif
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

  WebContents* tab = OpenWebAppWindow(launch_params, url);

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
