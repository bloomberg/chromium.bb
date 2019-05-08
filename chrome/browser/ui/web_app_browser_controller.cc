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

// static
bool WebAppBrowserController::IsSiteSecure(
    const content::WebContents* web_contents) {
  const SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  if (helper) {
    switch (helper->GetSecurityLevel()) {
      case security_state::SECURITY_LEVEL_COUNT:
        NOTREACHED();
        return false;
      case security_state::EV_SECURE:
      case security_state::SECURE:
      case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
        return true;
      case security_state::NONE:
      case security_state::HTTP_SHOW_WARNING:
      case security_state::DANGEROUS:
        return false;
    }
  }
  return false;
}

WebAppBrowserController::WebAppBrowserController(Browser* browser)
    : content::WebContentsObserver(nullptr), browser_(browser) {
  browser->tab_strip_model()->AddObserver(this);
}

WebAppBrowserController::~WebAppBrowserController() {
  browser()->tab_strip_model()->RemoveObserver(this);
}

bool WebAppBrowserController::IsForExperimentalWebAppBrowser() const {
  return base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing) ||
         base::FeatureList::IsEnabled(::features::kFocusMode);
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

void WebAppBrowserController::Uninstall() {
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

base::Optional<SkColor> WebAppBrowserController::GetThemeColor() const {
  base::Optional<SkColor> result;
  // HTML meta theme-color tag overrides manifest theme_color, see spec:
  // https://www.w3.org/TR/appmanifest/#theme_color-member
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (web_contents) {
    base::Optional<SkColor> color = web_contents->GetThemeColor();
    if (color)
      result = color;
  }

  if (!result)
    return base::nullopt;

  // The frame/tabstrip code expects an opaque color.
  return SkColorSetA(*result, SK_AlphaOPAQUE);
}

base::string16 WebAppBrowserController::GetTitle() const {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::string16();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  return entry ? entry->GetTitle() : base::string16();
}

void WebAppBrowserController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      OnTabInserted(contents.contents);
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents)
      OnTabRemoved(contents.contents);
  }
}

void WebAppBrowserController::OnTabInserted(content::WebContents* contents) {
  DCHECK(!web_contents()) << " App windows are single tabbed only";
  content::WebContentsObserver::Observe(contents);
  DidChangeThemeColor(GetThemeColor());
}

void WebAppBrowserController::OnTabRemoved(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents());
  content::WebContentsObserver::Observe(nullptr);
}
