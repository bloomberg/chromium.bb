// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_factory.h"

#include "base/bind.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_view.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

MockPermissionBubbleFactory::MockPermissionBubbleFactory(
    bool is_browser_test,
    PermissionBubbleManager* manager)
    : is_browser_test_(is_browser_test),
      can_update_ui_(false),
      show_count_(0),
      requests_count_(0),
      total_requests_count_(0),
      response_type_(PermissionBubbleManager::NONE),
      manager_(manager) {
  manager->view_factory_ =
      base::Bind(&MockPermissionBubbleFactory::Create, base::Unretained(this));
}

MockPermissionBubbleFactory::~MockPermissionBubbleFactory() {
  manager_->view_factory_ =
      base::Bind(&MockPermissionBubbleFactory::DoNotCreate);
  while (!views_.empty()) {
    views_.back()->factory_ = nullptr;
    views_.pop_back();
  }
}

scoped_ptr<PermissionBubbleView> MockPermissionBubbleFactory::Create(
    Browser* browser) {
  MockPermissionBubbleView* view =
      new MockPermissionBubbleView(this, manager_, is_browser_test_);
  view->can_update_ui_ = can_update_ui_;
  return make_scoped_ptr(view);
}

void MockPermissionBubbleFactory::SetCanUpdateUi(bool can_update_ui) {
  can_update_ui_ = can_update_ui;
  for (auto* view : views_)
    view->can_update_ui_ = can_update_ui_;
}

void MockPermissionBubbleFactory::ResetCounts() {
  show_count_ = 0;
  requests_count_ = 0;
  total_requests_count_ = 0;
}

void MockPermissionBubbleFactory::DocumentOnLoadCompletedInMainFrame() {
  manager_->DocumentOnLoadCompletedInMainFrame();
}

bool MockPermissionBubbleFactory::is_visible() {
  for (auto* view : views_) {
    if (view->IsVisible())
      return true;
  }
  return false;
}

// static
scoped_ptr<PermissionBubbleView> MockPermissionBubbleFactory::DoNotCreate(
    Browser* browser) {
  NOTREACHED();
  return make_scoped_ptr(new MockPermissionBubbleView(nullptr, nullptr, false));
}

void MockPermissionBubbleFactory::UpdateResponseType() {
  manager_->set_auto_response_for_test(response_type_);
}

void MockPermissionBubbleFactory::ShowView(MockPermissionBubbleView* view) {
  for (std::vector<MockPermissionBubbleView*>::iterator iter = views_.begin();
       iter != views_.end(); ++iter) {
    if (*iter == view)
      return;
  }
  views_.push_back(view);
}

void MockPermissionBubbleFactory::HideView(MockPermissionBubbleView* view) {
  for (std::vector<MockPermissionBubbleView*>::iterator iter = views_.begin();
       iter != views_.end(); ++iter) {
    if (*iter == view) {
      views_.erase(iter);
      return;
    }
  }
}
