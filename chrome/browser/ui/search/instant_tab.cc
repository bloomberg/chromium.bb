// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

InstantTab::Delegate::~Delegate() {
}

InstantTab::~InstantTab() {
  if (web_contents()) {
    SearchTabHelper::FromWebContents(web_contents())->model()->RemoveObserver(
        this);
  }
}

bool InstantTab::supports_instant() const {
  return web_contents() &&
      SearchTabHelper::FromWebContents(web_contents())->SupportsInstant();
}

bool InstantTab::IsLocal() const {
  return web_contents() &&
      web_contents()->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl);
}

InstantTab::InstantTab(Delegate* delegate)
    : delegate_(delegate) {
}

void InstantTab::Init(content::WebContents* new_web_contents) {
  ClearContents();

  if (!new_web_contents)
    return;

  Observe(new_web_contents);
  SearchModel* model =
      SearchTabHelper::FromWebContents(web_contents())->model();
  model->AddObserver(this);

  // Already know whether the page supports instant.
  if (model->instant_support() != INSTANT_SUPPORT_UNKNOWN)
    InstantSupportDetermined(model->instant_support() == INSTANT_SUPPORT_YES);
}

void InstantTab::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition /* transition_type */) {
  if (!render_frame_host->GetParent()) {
    delegate_->InstantTabAboutToNavigateMainFrame(web_contents(), url);
  }
}

void InstantTab::ModelChanged(const SearchModel::State& old_state,
                               const SearchModel::State& new_state) {
  if (old_state.instant_support != new_state.instant_support)
    InstantSupportDetermined(new_state.instant_support == INSTANT_SUPPORT_YES);
}

void InstantTab::InstantSupportDetermined(bool supports_instant) {
  delegate_->InstantSupportDetermined(web_contents(), supports_instant);

  // If the page doesn't support Instant, stop listening to it.
  if (!supports_instant)
    ClearContents();
}

void InstantTab::ClearContents() {
  if (web_contents()) {
    SearchTabHelper::FromWebContents(web_contents())->model()->RemoveObserver(
        this);
  }

  Observe(NULL);
}
