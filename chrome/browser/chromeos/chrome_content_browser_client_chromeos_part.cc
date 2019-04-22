// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_content_browser_client_chromeos_part.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"

namespace {

GURL GetURL(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  return entry ? entry->GetURL() : GURL();
}

// Returns true if |contents| is of an internal pages (such as
// chrome://settings, chrome://extensions, ... etc) or the New Tab Page.
bool ShouldExcludePage(content::WebContents* contents) {
  DCHECK(contents);

  GURL url = GetURL(contents);
  if (url.is_empty()) {
    // No entry has been committed. We don't know anything about this page, so
    // exclude it and let it have the default web prefs.
    return true;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (profile && search::IsNTPOrRelatedURL(url, profile))
    return true;

  return url.SchemeIs(content::kChromeUIScheme);
}

void OverrideWebkitPrefsForTabletMode(content::WebContents* contents,
                                      content::WebPreferences* web_prefs) {
  // Enable some mobile-like behaviors when in tablet mode on Chrome OS.
  if (!TabletModeClient::Get() ||
      !TabletModeClient::Get()->tablet_mode_enabled()) {
    return;
  }

  // Do this only for webcontents displayed in browsers and are not of hosted
  // apps.
  auto* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser || browser->is_app())
    return;

  // Also exclude internal pages and NTPs.
  if (ShouldExcludePage(contents))
    return;

  web_prefs->double_tap_to_zoom_enabled =
      base::FeatureList::IsEnabled(features::kDoubleTapToZoomInTabletMode);
  web_prefs->text_autosizing_enabled = true;
  web_prefs->shrinks_viewport_contents_to_fit = true;
  web_prefs->main_frame_resizes_are_orientation_changes = true;
  web_prefs->default_minimum_page_scale_factor = 0.25f;
  web_prefs->default_maximum_page_scale_factor = 5.0;
}

void OverrideWebkitPrefsForSystemDialogs(content::WebContents* contents,
                                         content::WebPreferences* web_prefs) {
  DCHECK(contents);
  if (!chromeos::features::IsSplitSettingsEnabled())
    return;
  GURL url = GetURL(contents);
  if (!url.is_empty() && chromeos::SystemWebDialogDelegate::HasInstance(url)) {
    // System dialogs are considered native UI, so they do not follow the
    // browser's web-page font sizes. Reset fonts to the base sizes.
    content::WebPreferences base_prefs;
    web_prefs->default_font_size = base_prefs.default_font_size;
    web_prefs->default_fixed_font_size = base_prefs.default_fixed_font_size;
  }
}

}  // namespace

ChromeContentBrowserClientChromeOsPart ::
    ChromeContentBrowserClientChromeOsPart() = default;

ChromeContentBrowserClientChromeOsPart ::
    ~ChromeContentBrowserClientChromeOsPart() = default;

void ChromeContentBrowserClientChromeOsPart::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(rvh);

  // A webcontents may not be the delegate of the render view host such as in
  // the case of interstitial pages.
  if (!contents)
    return;

  OverrideWebkitPrefsForTabletMode(contents, web_prefs);
  OverrideWebkitPrefsForSystemDialogs(contents, web_prefs);
}
