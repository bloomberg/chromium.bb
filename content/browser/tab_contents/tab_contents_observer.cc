// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_observer.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"

void TabContentsObserver::RenderViewCreated(RenderViewHost* render_view_host) {
}

void TabContentsObserver::RenderViewDeleted(RenderViewHost* render_view_host) {
}

void TabContentsObserver::RenderViewReady() {
}

void TabContentsObserver::RenderViewGone(base::TerminationStatus status) {
}

void TabContentsObserver::NavigateToPendingEntry(
    const GURL& url,
    NavigationController::ReloadType reload_type) {
}

void TabContentsObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
}

void TabContentsObserver::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
}

void TabContentsObserver::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    RenderViewHost* render_view_host) {
}

void TabContentsObserver::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    const GURL& opener_url) {
}

void TabContentsObserver::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type) {
}

void TabContentsObserver::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description) {
}

void TabContentsObserver::DocumentAvailableInMainFrame() {
}

void TabContentsObserver::DocumentLoadedInFrame(int64 frame_id) {
}

void TabContentsObserver::DidFinishLoad(
     int64 frame_id,
     const GURL& validated_url,
     bool is_main_frame) {
}

void TabContentsObserver::DidFailLoad(int64 frame_id,
                                      const GURL& validated_url,
                                      bool is_main_frame,
                                      int error_code,
                                      const string16& error_description) {
}

void TabContentsObserver::DidGetUserGesture() {
}

void TabContentsObserver::DidGetIgnoredUIEvent() {
}

void TabContentsObserver::DidBecomeSelected() {
}

void TabContentsObserver::DidStartLoading() {
}

void TabContentsObserver::DidStopLoading() {
}

void TabContentsObserver::StopNavigation() {
}

void TabContentsObserver::DidOpenURL(const GURL& url,
                                     const GURL& referrer,
                                     WindowOpenDisposition disposition,
                                     content::PageTransition transition) {
}

void TabContentsObserver::DidOpenRequestedURL(
    TabContents* new_contents,
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    content::PageTransition transition,
    int64 source_frame_id) {
}

void TabContentsObserver::AppCacheAccessed(const GURL& manifest_url,
                                           bool blocked_by_policy) {
}

TabContentsObserver::TabContentsObserver(TabContents* tab_contents)
    : tab_contents_(NULL) {
  Observe(tab_contents);
}

TabContentsObserver::TabContentsObserver()
    : tab_contents_(NULL) {
}

TabContentsObserver::~TabContentsObserver() {
  if (tab_contents_)
    tab_contents_->RemoveObserver(this);
}

void TabContentsObserver::Observe(TabContents* tab_contents) {
  if (tab_contents_)
    tab_contents_->RemoveObserver(this);
  tab_contents_ = tab_contents;
  if (tab_contents_) {
    tab_contents_->AddObserver(this);
  }
}

void TabContentsObserver::TabContentsDestroyed(TabContents* tab) {
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

int TabContentsObserver::routing_id() const {
  if (!tab_contents_ || !tab_contents_->render_view_host())
    return MSG_ROUTING_NONE;

  return tab_contents_->render_view_host()->routing_id();
}

void TabContentsObserver::TabContentsDestroyed() {
  // Do cleanup so that 'this' can safely be deleted from
  // TabContentsDestroyed.
  tab_contents_->RemoveObserver(this);
  TabContents* tab = tab_contents_;
  tab_contents_ = NULL;
  TabContentsDestroyed(tab);
}
