// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_delegate.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"

namespace chrome {
namespace search {

SearchDelegate::SearchDelegate(SearchModel* browser_search_model,
                               ToolbarModel* toolbar_model)
    : browser_model_(browser_search_model),
      tab_model_() {
}

SearchDelegate::~SearchDelegate() {
  DCHECK(!tab_model_) << "All tabs should have been deactivated or closed.";
}

void SearchDelegate::ModeChanged(const Mode& old_mode, const Mode& new_mode) {
  browser_model_->SetMode(new_mode);
}

void SearchDelegate::OnTabActivated(content::WebContents* web_contents) {
  if (tab_model_)
    tab_model_->RemoveObserver(this);
  tab_model_ =
      chrome::search::SearchTabHelper::FromWebContents(web_contents)->model();
  browser_model_->SetMode(tab_model_->mode());
  tab_model_->AddObserver(this);
}

void SearchDelegate::OnTabDeactivated(content::WebContents* web_contents) {
  StopObservingTab(web_contents);
}

void SearchDelegate::OnTabDetached(content::WebContents* web_contents) {
  StopObservingTab(web_contents);
}

void SearchDelegate::StopObservingTab(content::WebContents* web_contents) {
  chrome::search::SearchTabHelper* search_tab_helper =
      chrome::search::SearchTabHelper::FromWebContents(web_contents);
  if (search_tab_helper->model() == tab_model_) {
    tab_model_->RemoveObserver(this);
    tab_model_ = NULL;
  }
}

}  // namespace search
}  // namespace chrome
