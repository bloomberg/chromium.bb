// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_observer.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

TabContentsObserver::Registrar::Registrar(TabContentsObserver* observer)
    : observer_(observer), tab_(NULL) {
}

TabContentsObserver::Registrar::~Registrar() {
  if (tab_)
    tab_->RemoveObserver(observer_);
}

void TabContentsObserver::Registrar::Observe(TabContents* tab) {
  observer_->SetTabContents(tab);
  if (tab_)
    tab_->RemoveObserver(observer_);
  tab_ = tab;
  if (tab_)
    tab_->AddObserver(observer_);
}

void TabContentsObserver::NavigateToPendingEntry(
    const GURL& url,
    NavigationController::ReloadType reload_type) {
}

void TabContentsObserver::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
}

void TabContentsObserver::DidNavigateAnyFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
}

void TabContentsObserver::OnProvisionalChangeToMainFrameUrl(const GURL& url) {
}

void TabContentsObserver::DidStartLoading() {
}

void TabContentsObserver::DidStopLoading() {
}

void TabContentsObserver::RenderViewGone() {
}

void TabContentsObserver::StopNavigation() {
}

TabContentsObserver::TabContentsObserver(TabContents* tab_contents) {
  SetTabContents(tab_contents);
  tab_contents_->AddObserver(this);
}

TabContentsObserver::TabContentsObserver()
    : tab_contents_(NULL), routing_id_(MSG_ROUTING_NONE) {
}

TabContentsObserver::~TabContentsObserver() {
  if (tab_contents_)
    tab_contents_->RemoveObserver(this);
}

void TabContentsObserver::OnTabContentsDestroyed(TabContents* tab) {
}

bool TabContentsObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool TabContentsObserver::Send(IPC::Message* message) {
  if (!tab_contents_ || !tab_contents_->render_view_host()) {
    delete message;
    return false;
  }

  return tab_contents_->render_view_host()->Send(message);
}

void TabContentsObserver::SetTabContents(TabContents* tab_contents) {
  tab_contents_ = tab_contents;
  if (tab_contents_)
    routing_id_ = tab_contents->render_view_host()->routing_id();
}

void TabContentsObserver::TabContentsDestroyed() {
  // Do cleanup so that 'this' can safely be deleted from
  // OnTabContentsDestroyed.
  tab_contents_->RemoveObserver(this);
  TabContents* tab = tab_contents_;
  tab_contents_ = NULL;
  OnTabContentsDestroyed(tab);
}
