// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_view.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_factory.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

MockPermissionBubbleView::~MockPermissionBubbleView() {
  Hide();
}

void MockPermissionBubbleView::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state) {
  factory_->ShowView(this);
  factory_->show_count_++;
  factory_->requests_count_ = manager_->requests_.size();
  factory_->total_requests_count_ += manager_->requests_.size();
  factory_->UpdateResponseType();
  is_visible_ = true;

  if (browser_test_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

bool MockPermissionBubbleView::CanAcceptRequestUpdate() {
  return can_update_ui_;
}

void MockPermissionBubbleView::Hide() {
  if (is_visible_ && factory_)
    factory_->HideView(this);
  is_visible_ = false;
}

bool MockPermissionBubbleView::IsVisible() {
  return is_visible_;
}

void MockPermissionBubbleView::UpdateAnchorPosition() {}

gfx::NativeWindow MockPermissionBubbleView::GetNativeWindow() {
  // This class should only be used when the UI is not necessary.
  NOTREACHED();
  return nullptr;
}

MockPermissionBubbleView::MockPermissionBubbleView(
    MockPermissionBubbleFactory* factory,
    PermissionBubbleManager* manager,
    bool browser_test)
    : factory_(factory),
      manager_(manager),
      browser_test_(browser_test),
      can_update_ui_(true),
      is_visible_(false) {}
