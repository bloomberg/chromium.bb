// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_app_browser_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// static
bool WebAppBrowserController::IsForExperimentalWebAppBrowser(
    const Browser* browser) {
  return browser && browser->web_app_controller() &&
         browser->web_app_controller()->IsForExperimentalWebAppBrowser();
}

// static
base::string16 WebAppBrowserController::FormatUrlOrigin(const GURL& url) {
  return url_formatter::FormatUrl(
      url.GetOrigin(),
      url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitHTTP |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

WebAppBrowserController::WebAppBrowserController(Browser* browser)
    : content::WebContentsObserver(nullptr), browser_(browser) {}

WebAppBrowserController::~WebAppBrowserController() = default;

bool WebAppBrowserController::IsForExperimentalWebAppBrowser() const {
  return base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing);
}

bool WebAppBrowserController::CreatedForInstalledPwa() const {
  return false;
}

bool WebAppBrowserController::IsInstalled() const {
  return false;
}

bool WebAppBrowserController::IsHostedApp() const {
  return false;
}

bool WebAppBrowserController::CanUninstall() const {
  return false;
}

void WebAppBrowserController::Uninstall(extensions::UninstallReason reason,
                                        extensions::UninstallSource source) {
  NOTREACHED();
  return;
}

void WebAppBrowserController::UpdateToolbarVisibility(bool animate) const {
  browser()->window()->UpdateToolbarVisibility(ShouldShowToolbar(), animate);
}

void WebAppBrowserController::DidChangeThemeColor(
    base::Optional<SkColor> theme_color) {
  browser_->window()->UpdateFrameColor();
}
