// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_prompt_factory.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/website_settings/mock_permission_prompt.h"
#include "content/public/browser/web_contents.h"

MockPermissionPromptFactory::MockPermissionPromptFactory(
    PermissionRequestManager* manager)
    : can_update_ui_(false),
      show_count_(0),
      requests_count_(0),
      total_requests_count_(0),
      response_type_(PermissionRequestManager::NONE),
      manager_(manager) {
  manager->view_factory_ =
      base::Bind(&MockPermissionPromptFactory::Create, base::Unretained(this));
}

MockPermissionPromptFactory::~MockPermissionPromptFactory() {
  manager_->view_factory_ =
      base::Bind(&MockPermissionPromptFactory::DoNotCreate);
  for (auto* prompt : prompts_)
    prompt->factory_ = nullptr;
  prompts_.clear();
}

std::unique_ptr<PermissionPrompt> MockPermissionPromptFactory::Create(
    content::WebContents* web_contents) {
  MockPermissionPrompt* prompt = new MockPermissionPrompt(this, manager_);
  prompt->can_update_ui_ = can_update_ui_;
  return base::WrapUnique(prompt);
}

void MockPermissionPromptFactory::SetCanUpdateUi(bool can_update_ui) {
  can_update_ui_ = can_update_ui;
  for (auto* prompt : prompts_)
    prompt->can_update_ui_ = can_update_ui_;
}

void MockPermissionPromptFactory::ResetCounts() {
  show_count_ = 0;
  requests_count_ = 0;
  total_requests_count_ = 0;
}

void MockPermissionPromptFactory::DocumentOnLoadCompletedInMainFrame() {
  manager_->DocumentOnLoadCompletedInMainFrame();
}

bool MockPermissionPromptFactory::is_visible() {
  for (auto* prompt : prompts_) {
    if (prompt->IsVisible())
      return true;
  }
  return false;
}

void MockPermissionPromptFactory::WaitForPermissionBubble() {
  if (is_visible())
    return;
  DCHECK(show_bubble_quit_closure_.is_null());
  base::RunLoop loop;
  show_bubble_quit_closure_ = loop.QuitClosure();
  loop.Run();
  show_bubble_quit_closure_ = base::Closure();
}

// static
std::unique_ptr<PermissionPrompt> MockPermissionPromptFactory::DoNotCreate(
    content::WebContents* web_contents) {
  NOTREACHED();
  return base::WrapUnique(new MockPermissionPrompt(nullptr, nullptr));
}

void MockPermissionPromptFactory::UpdateResponseType() {
  manager_->set_auto_response_for_test(response_type_);
}

void MockPermissionPromptFactory::ShowView(MockPermissionPrompt* prompt) {
  if (base::ContainsValue(prompts_, prompt))
    return;
  prompts_.push_back(prompt);

  if (!show_bubble_quit_closure_.is_null())
    show_bubble_quit_closure_.Run();
}

void MockPermissionPromptFactory::HideView(MockPermissionPrompt* prompt) {
  auto it = std::find(prompts_.begin(), prompts_.end(), prompt);
  if (it != prompts_.end())
    prompts_.erase(it);
}
