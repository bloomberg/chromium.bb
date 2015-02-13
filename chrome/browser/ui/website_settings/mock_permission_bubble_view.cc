// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_view.h"

#include "base/run_loop.h"

MockPermissionBubbleView::MockPermissionBubbleView()
    : visible_(false),
      can_accept_updates_(true),
      delegate_(nullptr),
      browser_test_(false) {}

MockPermissionBubbleView::~MockPermissionBubbleView() {}

void MockPermissionBubbleView::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void MockPermissionBubbleView::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state) {
  visible_ = true;
  permission_requests_ = requests;
  permission_states_ = accept_state;
  if (browser_test_)
    base::MessageLoopForUI::current()->Quit();
}

void MockPermissionBubbleView::Hide() {
  visible_ = false;
}

bool MockPermissionBubbleView::IsVisible() {
  return visible_;
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
