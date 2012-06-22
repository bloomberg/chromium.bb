// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_delegate.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

namespace chrome {
namespace search {

SearchDelegate::SearchDelegate(SearchModel* browser_model)
    : browser_model_(browser_model),
      tab_model_(NULL) {
}

SearchDelegate::~SearchDelegate() {
  DCHECK(!tab_model_) << "All tabs should have been deactivated or closed.";
}

void SearchDelegate::ModeChanged(const Mode& mode) {
  browser_model_->SetMode(mode);
}

void SearchDelegate::OnTabActivated(TabContents* contents) {
  if (tab_model_)
    tab_model_->RemoveObserver(this);
  tab_model_ = contents->search_tab_helper()->model();
  DCHECK_EQ(tab_model_->tab_contents(), contents);
  browser_model_->set_tab_contents(contents);
  browser_model_->SetMode(tab_model_->mode());
  tab_model_->AddObserver(this);
}

void SearchDelegate::OnTabDeactivated(TabContents* contents) {
  StopObserveringTab(contents);
}

void SearchDelegate::OnTabDetached(TabContents* contents) {
  StopObserveringTab(contents);
}

void SearchDelegate::StopObserveringTab(
    TabContents* contents) {
  if (contents->search_tab_helper()->model() == tab_model_) {
    tab_model_->RemoveObserver(this);
    browser_model_->set_tab_contents(NULL);
    tab_model_ = NULL;
  }
}

}  // namespace search
}  // namespace chrome
