// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/permission_bubble/mock_permission_prompt.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "ui/gfx/vector_icon_types.h"
#endif

MockPermissionPrompt::~MockPermissionPrompt() {
  Hide();
}

void MockPermissionPrompt::Show() {
  factory_->ShowView(this);
  factory_->show_count_++;
  factory_->requests_count_ = manager_->requests_.size();
  for (const PermissionRequest* request : manager_->requests_) {
    factory_->request_types_seen_.push_back(
        request->GetPermissionRequestType());
    // The actual prompt will call these, so test they're sane.
    EXPECT_FALSE(request->GetMessageTextFragment().empty());
#if defined(OS_ANDROID)
    EXPECT_FALSE(request->GetMessageText().empty());
    EXPECT_NE(0, request->GetIconId());
#else
    EXPECT_FALSE(request->GetIconId().is_empty());
#endif
  }
  factory_->UpdateResponseType();
  is_visible_ = true;
}

bool MockPermissionPrompt::CanAcceptRequestUpdate() {
  return can_update_ui_;
}

bool MockPermissionPrompt::HidesAutomatically() {
  return false;
}

void MockPermissionPrompt::Hide() {
  if (is_visible_ && factory_)
    factory_->HideView(this);
  is_visible_ = false;
}

void MockPermissionPrompt::UpdateAnchorPosition() {}

gfx::NativeWindow MockPermissionPrompt::GetNativeWindow() {
  // This class should only be used when the UI is not necessary.
  NOTREACHED();
  return nullptr;
}

bool MockPermissionPrompt::IsVisible() {
  return is_visible_;
}

MockPermissionPrompt::MockPermissionPrompt(MockPermissionPromptFactory* factory,
                                           PermissionRequestManager* manager)
    : factory_(factory),
      manager_(manager),
      can_update_ui_(true),
      is_visible_(false) {}
