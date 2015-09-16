// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

bool IsSameOriginOrMoreSecure(const GURL& app_url, const GURL& page_url) {
  const std::string www("www.");
  return (app_url.scheme() == page_url.scheme() ||
          page_url.scheme() == url::kHttpsScheme) &&
         (app_url.host() == page_url.host() ||
          www + app_url.host() == page_url.host()) &&
         app_url.port() == page_url.port();
}

bool IsWebAppFrameEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAppFrame);
}

}  // namespace

// static
bool HostedAppBrowserController::IsForHostedApp(Browser* browser) {
  const std::string extension_id =
      web_app::GetExtensionIdFromApplicationName(browser->app_name());
  const Extension* extension =
      ExtensionRegistry::Get(browser->profile())->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  return extension && extension->is_hosted_app();
}

HostedAppBrowserController::HostedAppBrowserController(Browser* browser)
    : browser_(browser),
      extension_id_(
          web_app::GetExtensionIdFromApplicationName(browser->app_name())) {
  const Extension* extension =
      ExtensionRegistry::Get(browser->profile())->GetExtensionById(
          extension_id_, ExtensionRegistry::EVERYTHING);
  DCHECK(extension);
  should_use_web_app_frame_ =
      extension->from_bookmark() &&
      browser->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH &&
      IsWebAppFrameEnabled();
}

HostedAppBrowserController::~HostedAppBrowserController() {
}

bool HostedAppBrowserController::SupportsLocationBar() const {
  return !should_use_web_app_frame();
}

bool HostedAppBrowserController::ShouldShowLocationBar() const {
  const Extension* extension =
      ExtensionRegistry::Get(browser_->profile())->GetExtensionById(
          extension_id_, ExtensionRegistry::EVERYTHING);

  const content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  // Default to not showing the location bar if either |extension| or
  // |web_contents| are null. |extension| is null for the dev tools.
  if (!extension || !web_contents)
    return false;

  if (!extension->is_hosted_app())
    return false;

  // Don't show a location bar until a navigation has occurred.
  if (web_contents->GetLastCommittedURL().is_empty())
    return false;

  const SecurityStateModel* model =
      SecurityStateModel::FromWebContents(web_contents);
  if (model &&
      model->GetSecurityInfo().security_level ==
          SecurityStateModel::SECURITY_ERROR)
    return true;

  GURL launch_url = AppLaunchInfo::GetLaunchWebURL(extension);
  return !(IsSameOriginOrMoreSecure(launch_url,
                                    web_contents->GetVisibleURL()) &&
           IsSameOriginOrMoreSecure(launch_url,
                                    web_contents->GetLastCommittedURL()));
}

void HostedAppBrowserController::UpdateLocationBarVisibility(
    bool animate) const {
  if (!SupportsLocationBar())
    return;

  browser_->window()->GetLocationBar()->UpdateLocationBarVisibility(
      ShouldShowLocationBar(), animate);
}

}  // namespace extensions
