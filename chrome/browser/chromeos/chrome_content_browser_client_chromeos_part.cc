// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_content_browser_client_chromeos_part.h"

#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"

ChromeContentBrowserClientChromeOsPart ::
    ChromeContentBrowserClientChromeOsPart() = default;

ChromeContentBrowserClientChromeOsPart ::
    ~ChromeContentBrowserClientChromeOsPart() = default;

void ChromeContentBrowserClientChromeOsPart::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!TabletModeClient::Get() ||
      !TabletModeClient::Get()->tablet_mode_enabled() ||
      !chrome::FindBrowserWithWebContents(contents)) {
    return;
  }

  // Enable some mobile-like behaviors when in tablet mode on Chrome OS. Do
  // this only for webcontents displayed in browsers.
  web_prefs->viewport_enabled = true;
  web_prefs->viewport_meta_enabled = true;
  web_prefs->shrinks_viewport_contents_to_fit = true;
  web_prefs->viewport_style = content::ViewportStyle::MOBILE;
  web_prefs->main_frame_resizes_are_orientation_changes = true;
  web_prefs->default_minimum_page_scale_factor = 0.25f;
  web_prefs->default_maximum_page_scale_factor = 5.0;
}
