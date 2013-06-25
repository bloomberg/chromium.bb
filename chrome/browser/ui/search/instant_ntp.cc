// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_ntp.h"

#include "chrome/browser/ui/search/search_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

InstantNTP::InstantNTP(InstantPage::Delegate* delegate,
                       const std::string& instant_url,
                       bool is_incognito)
    : InstantPage(delegate, instant_url, is_incognito),
      loader_(this) {
}

InstantNTP::~InstantNTP() {
}

void InstantNTP::InitContents(Profile* profile,
                              const content::WebContents* active_tab,
                              const base::Closure& on_stale_callback) {
  loader_.Init(GURL(instant_url()), profile, active_tab, on_stale_callback);
  SetContents(loader_.contents());
  SearchTabHelper::FromWebContents(contents())->InitForPreloadedNTP();
  loader_.Load();
}

scoped_ptr<content::WebContents> InstantNTP::ReleaseContents() {
  SetContents(NULL);
  return loader_.ReleaseContents();
}

void InstantNTP::OnSwappedContents() {
  SetContents(loader_.contents());
}

void InstantNTP::OnFocus() {
  NOTREACHED();
}

void InstantNTP::OnMouseDown() {
  NOTREACHED();
}

void InstantNTP::OnMouseUp() {
  NOTREACHED();
}

content::WebContents* InstantNTP::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return NULL;
}

void InstantNTP::LoadCompletedMainFrame() {
}

bool InstantNTP::ShouldProcessRenderViewCreated() {
  return true;
}

bool InstantNTP::ShouldProcessRenderViewGone() {
  return true;
}
