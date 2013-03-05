// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_ntp.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

InstantNTP::InstantNTP(InstantPage::Delegate* delegate,
                       const std::string& instant_url)
    : InstantPage(delegate),
      loader_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      instant_url_(instant_url) {
}

InstantNTP::~InstantNTP() {
}

void InstantNTP::InitContents(Profile* profile,
                              const content::WebContents* active_tab,
                              const base::Closure& on_stale_callback) {
  loader_.Init(GURL(instant_url_), profile, active_tab, on_stale_callback);
  SetContents(loader_.contents());
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

bool InstantNTP::ShouldProcessRenderViewCreated() {
  return true;
}

bool InstantNTP::ShouldProcessRenderViewGone() {
  return true;
}
