// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_container.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/bindings_policy.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

SidebarContainer::SidebarContainer(TabContents* tab,
                                   const std::string& content_id,
                                   Delegate* delegate)
    : tab_(tab),
      content_id_(content_id),
      delegate_(delegate),
      icon_(new SkBitmap) {
  // Create TabContents for sidebar.
  sidebar_contents_.reset(
      new TabContents(tab->profile(), NULL, MSG_ROUTING_NONE, NULL));
  sidebar_contents_->render_view_host()->set_is_extension_process(true);
  sidebar_contents_->render_view_host()->AllowBindings(
      BindingsPolicy::EXTENSION);
  sidebar_contents_->set_delegate(this);
}

SidebarContainer::~SidebarContainer() {
}

void SidebarContainer::SidebarClosing() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Show() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Expand() {
  delegate_->UpdateSidebar(this);
  sidebar_contents_->view()->SetInitialFocus();
}

void SidebarContainer::Collapse() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Navigate(const GURL& url) {
  DCHECK(sidebar_contents_.get());
  // TODO(alekseys): add a progress UI.
  sidebar_contents_->controller().LoadURL(
      url, GURL(), PageTransition::START_PAGE);
}

void SidebarContainer::SetBadgeText(const string16& badge_text) {
  badge_text_ = badge_text;
}

void SidebarContainer::SetIcon(const SkBitmap& bitmap) {
  *icon_ = bitmap;
}

void SidebarContainer::SetTitle(const string16& title) {
  title_ = title;
}
