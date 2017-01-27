// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

InstantTab::Delegate::~Delegate() {}

InstantTab::InstantTab(Delegate* delegate, content::WebContents* web_contents)
    : delegate_(delegate), pending_web_contents_(web_contents) {}

InstantTab::~InstantTab() {
  if (web_contents()) {
    SearchTabHelper::FromWebContents(web_contents())->model()->RemoveObserver(
        this);
  }
}

void InstantTab::Init() {
  if (!pending_web_contents_)
    return;

  Observe(pending_web_contents_);
  pending_web_contents_ = nullptr;

  SearchModel* model =
      SearchTabHelper::FromWebContents(web_contents())->model();
  model->AddObserver(this);

  // Already know whether the page supports instant.
  if (model->instant_support() != INSTANT_SUPPORT_UNKNOWN)
    InstantSupportDetermined(model->instant_support() == INSTANT_SUPPORT_YES);
}

void InstantTab::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame()) {
    delegate_->InstantTabAboutToNavigateMainFrame(
        web_contents(), navigation_handle->GetURL());
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
  if (!supports_instant) {
    if (web_contents()) {
      SearchTabHelper::FromWebContents(web_contents())->model()->RemoveObserver(
          this);
    }

    Observe(nullptr);
  }
}
