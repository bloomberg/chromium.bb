// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_prompt.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/website_settings/mock_permission_prompt_factory.h"

MockPermissionPrompt::~MockPermissionPrompt() {
  Hide();
}

void MockPermissionPrompt::Show(const std::vector<PermissionRequest*>& requests,
                                const std::vector<bool>& accept_state) {
  factory_->ShowView(this);
  factory_->show_count_++;
  factory_->requests_count_ = manager_->requests_.size();
  factory_->total_requests_count_ += manager_->requests_.size();
  factory_->UpdateResponseType();
  is_visible_ = true;
}

bool MockPermissionPrompt::CanAcceptRequestUpdate() {
  return can_update_ui_;
}

void MockPermissionPrompt::Hide() {
  if (is_visible_ && factory_)
    factory_->HideView(this);
  is_visible_ = false;
}

bool MockPermissionPrompt::IsVisible() {
  return is_visible_;
}

void MockPermissionPrompt::UpdateAnchorPosition() {}

gfx::NativeWindow MockPermissionPrompt::GetNativeWindow() {
  // This class should only be used when the UI is not necessary.
  NOTREACHED();
  return nullptr;
}

MockPermissionPrompt::MockPermissionPrompt(MockPermissionPromptFactory* factory,
                                           PermissionRequestManager* manager)
    : factory_(factory),
      manager_(manager),
      can_update_ui_(true),
      is_visible_(false) {}
