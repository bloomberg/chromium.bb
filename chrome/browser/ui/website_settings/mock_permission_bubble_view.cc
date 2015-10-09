// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_view.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

MockPermissionBubbleView::~MockPermissionBubbleView() {}

// Static
scoped_ptr<PermissionBubbleView> MockPermissionBubbleView::CreateBrowserMock(
    Browser* browser) {
  return make_scoped_ptr(new MockPermissionBubbleView(true));
}

// Static
scoped_ptr<PermissionBubbleView> MockPermissionBubbleView::CreateUnitMock(
    Browser* browser) {
  return make_scoped_ptr(new MockPermissionBubbleView(false));
}

// Static
MockPermissionBubbleView* MockPermissionBubbleView::GetFrom(
    PermissionBubbleManager* manager) {
  return static_cast<MockPermissionBubbleView*>(manager->view_.get());
}

// Static
void MockPermissionBubbleView::SetFactory(PermissionBubbleManager* manager,
                                          bool is_browser_test) {
  if (is_browser_test) {
    manager->view_factory_ =
        base::Bind(&MockPermissionBubbleView::CreateBrowserMock);
  } else {
    manager->view_factory_ =
        base::Bind(&MockPermissionBubbleView::CreateUnitMock);
  }
  manager->HideBubble();
}

void MockPermissionBubbleView::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void MockPermissionBubbleView::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state) {
  visible_ = true;
  ++show_count_;
  requests_count_ += requests.size();
  permission_requests_ = requests;
  permission_states_ = accept_state;
  if (browser_test_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

void MockPermissionBubbleView::Hide() {
  visible_ = false;
}

bool MockPermissionBubbleView::IsVisible() {
  return visible_;
}

void MockPermissionBubbleView::UpdateAnchorPosition() {
}

gfx::NativeWindow MockPermissionBubbleView::GetNativeWindow() {
  return nullptr;
}

bool MockPermissionBubbleView::CanAcceptRequestUpdate() {
  return can_accept_updates_;
}

void MockPermissionBubbleView::Accept() {
  delegate_->Accept();
}

void MockPermissionBubbleView::Deny() {
  delegate_->Deny();
}

void MockPermissionBubbleView::Close() {
  delegate_->Closing();
}

void MockPermissionBubbleView::Clear() {
  visible_ = false;
  can_accept_updates_ = true;
  delegate_ = nullptr;
  permission_requests_.clear();
  permission_states_.clear();
}

void MockPermissionBubbleView::SetBrowserTest(bool browser_test) {
  browser_test_ = browser_test;
}

MockPermissionBubbleView::MockPermissionBubbleView(bool browser_test)
    : visible_(false),
      show_count_(0),
      requests_count_(0),
      can_accept_updates_(true),
      delegate_(nullptr),
      browser_test_(browser_test) {}
