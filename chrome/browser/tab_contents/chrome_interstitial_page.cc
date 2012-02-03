// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/chrome_interstitial_page.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

ChromeInterstitialPage::ChromeInterstitialPage(WebContents* tab,
                                               bool new_navigation,
                                               const GURL& url)
    : InterstitialPage(tab, new_navigation, url) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::RendererPreferences prefs;
  renderer_preferences_util::UpdateFromSystemSettings(&prefs, profile);
  set_renderer_preferences(prefs);
}

ChromeInterstitialPage::~ChromeInterstitialPage() {
}

void ChromeInterstitialPage::Show() {
  InterstitialPage::Show();

  render_view_host()->Send(
      new ChromeViewMsg_SetAsInterstitial(render_view_host()->routing_id()));
}
