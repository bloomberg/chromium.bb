// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_delegate.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"

SearchDelegate::SearchDelegate(SearchModel* browser_search_model)
    : browser_model_(browser_search_model),
      tab_model_() {
}

SearchDelegate::~SearchDelegate() {
  DCHECK(!tab_model_) << "All tabs should have been deactivated or closed.";
}

void SearchDelegate::ModelChanged(const SearchModel::State& old_state,
                                  const SearchModel::State& new_state) {
  browser_model_->SetState(new_state);
}

void SearchDelegate::OnTabActivated(content::WebContents* web_contents) {
  if (tab_model_)
    tab_model_->RemoveObserver(this);
  tab_model_ = SearchTabHelper::FromWebContents(web_contents)->model();
  browser_model_->SetState(tab_model_->state());
  tab_model_->AddObserver(this);
}

void SearchDelegate::OnTabDeactivated(content::WebContents* web_contents) {
  StopObservingTab(web_contents);
}

void SearchDelegate::OnTabDetached(content::WebContents* web_contents) {
  StopObservingTab(web_contents);
}

void SearchDelegate::StopObservingTab(content::WebContents* web_contents) {
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents);
  if (search_tab_helper->model() == tab_model_) {
    tab_model_->RemoveObserver(this);
    tab_model_ = NULL;
  }
}
