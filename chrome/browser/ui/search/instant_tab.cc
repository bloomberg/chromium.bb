// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

InstantTab::Delegate::~Delegate() {}

InstantTab::InstantTab(Delegate* delegate, content::WebContents* web_contents)
    : delegate_(delegate), pending_web_contents_(web_contents) {}

InstantTab::~InstantTab() {}

void InstantTab::Init() {
  if (!pending_web_contents_)
    return;

  Observe(pending_web_contents_);
  pending_web_contents_ = nullptr;
}

void InstantTab::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame()) {
    delegate_->InstantTabAboutToNavigateMainFrame(
        web_contents(), navigation_handle->GetURL());
  }
}
