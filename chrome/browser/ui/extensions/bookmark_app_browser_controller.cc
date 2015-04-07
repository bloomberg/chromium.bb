// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/bookmark_app_browser_controller.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
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
#include "url/gurl.h"

namespace extensions {

namespace {

bool IsSameOriginOrMoreSecure(const GURL& app_url, const GURL& page_url) {
  return (app_url.scheme() == page_url.scheme() ||
          page_url.scheme() == url::kHttpsScheme) &&
         app_url.host() == page_url.host() &&
         app_url.port() == page_url.port();
}

bool IsWebAppFrameEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebAppFrame);
}

}  // namespace

// static
bool BookmarkAppBrowserController::IsForBookmarkApp(Browser* browser) {
  const std::string extension_id =
      web_app::GetExtensionIdFromApplicationName(browser->app_name());
  const Extension* extension =
      ExtensionRegistry::Get(browser->profile())->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  return extension && extension->from_bookmark();
}

BookmarkAppBrowserController::BookmarkAppBrowserController(Browser* browser)
    : browser_(browser),
      extension_id_(
          web_app::GetExtensionIdFromApplicationName(browser->app_name())),
      should_use_web_app_frame_(browser->host_desktop_type() ==
                                    chrome::HOST_DESKTOP_TYPE_ASH &&
                                IsWebAppFrameEnabled()) {
}

BookmarkAppBrowserController::~BookmarkAppBrowserController() {
}

bool BookmarkAppBrowserController::SupportsLocationBar() {
  return !should_use_web_app_frame();
}

bool BookmarkAppBrowserController::ShouldShowLocationBar() {
  const Extension* extension =
      ExtensionRegistry::Get(browser_->profile())->GetExtensionById(
          extension_id_, ExtensionRegistry::EVERYTHING);

  const content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  // Default to not showing the location bar if either |extension| or
  // |web_contents| are null. |extension| is null for the dev tools.
  if (!extension || !web_contents)
    return false;

  if (!extension->from_bookmark())
    return false;

  // Don't show a location bar until a navigation has occurred.
  if (web_contents->GetLastCommittedURL().is_empty())
    return false;

  GURL launch_url = AppLaunchInfo::GetLaunchWebURL(extension);
  return !(IsSameOriginOrMoreSecure(launch_url,
                                    web_contents->GetVisibleURL()) &&
           IsSameOriginOrMoreSecure(launch_url,
                                    web_contents->GetLastCommittedURL()));
}

void BookmarkAppBrowserController::UpdateLocationBarVisibility(bool animate) {
  if (!SupportsLocationBar())
    return;

  browser_->window()->GetLocationBar()->UpdateLocationBarVisibility(
      ShouldShowLocationBar(), animate);
}

}  // namespace extensions
