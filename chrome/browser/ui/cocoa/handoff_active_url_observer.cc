// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/handoff_active_url_observer.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/handoff_active_url_observer_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

HandoffActiveURLObserver::HandoffActiveURLObserver(
    HandoffActiveURLObserverDelegate* delegate)
    : delegate_(delegate),
      active_tab_strip_model_(nullptr),
      active_browser_(nullptr) {
  DCHECK(delegate_);

  active_browser_ = chrome::FindLastActiveWithHostDesktopType(
      chrome::HOST_DESKTOP_TYPE_NATIVE);
  BrowserList::AddObserver(this);
  UpdateObservations();
}

HandoffActiveURLObserver::~HandoffActiveURLObserver() {
  BrowserList::RemoveObserver(this);
  StopObservingTabStripModel();
  StopObservingWebContents();
}

void HandoffActiveURLObserver::OnBrowserSetLastActive(Browser* browser) {
  active_browser_ = browser;
  UpdateObservations();
  delegate_->HandoffActiveURLChanged(GetActiveWebContents());
}

void HandoffActiveURLObserver::OnBrowserRemoved(Browser* removed_browser) {
  if (active_browser_ != removed_browser)
    return;

  active_browser_ = chrome::FindLastActiveWithHostDesktopType(
      chrome::HOST_DESKTOP_TYPE_NATIVE);
  UpdateObservations();
  delegate_->HandoffActiveURLChanged(GetActiveWebContents());
}

void HandoffActiveURLObserver::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  StartObservingWebContents(new_contents);
  delegate_->HandoffActiveURLChanged(new_contents);
}

void HandoffActiveURLObserver::TabStripModelDeleted() {
  StopObservingTabStripModel();
  StopObservingWebContents();
}

void HandoffActiveURLObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  delegate_->HandoffActiveURLChanged(web_contents());
}

void HandoffActiveURLObserver::WebContentsDestroyed() {
  StopObservingWebContents();
}

void HandoffActiveURLObserver::UpdateObservations() {
  if (!active_browser_) {
    StopObservingTabStripModel();
    StopObservingWebContents();
    return;
  }

  TabStripModel* model = active_browser_->tab_strip_model();
  StartObservingTabStripModel(model);

  content::WebContents* web_contents = model->GetActiveWebContents();
  if (web_contents)
    StartObservingWebContents(web_contents);
  else
    StopObservingWebContents();
}

void HandoffActiveURLObserver::StartObservingTabStripModel(
    TabStripModel* tab_strip_model) {
  DCHECK(tab_strip_model);

  if (active_tab_strip_model_ == tab_strip_model)
    return;

  StopObservingTabStripModel();
  tab_strip_model->AddObserver(this);
  active_tab_strip_model_ = tab_strip_model;
}

void HandoffActiveURLObserver::StopObservingTabStripModel() {
  if (active_tab_strip_model_) {
    active_tab_strip_model_->RemoveObserver(this);
    active_tab_strip_model_ = nullptr;
  }
}

void HandoffActiveURLObserver::StartObservingWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Observe(web_contents);
}

void HandoffActiveURLObserver::StopObservingWebContents() {
  Observe(nullptr);
}

content::WebContents* HandoffActiveURLObserver::GetActiveWebContents() {
  if (!active_browser_)
    return nullptr;

  return active_browser_->tab_strip_model()->GetActiveWebContents();
}
