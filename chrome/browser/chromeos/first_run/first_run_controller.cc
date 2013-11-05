// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_controller.h"

#include "ash/first_run/first_run_helper.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/first_run/first_run_view.h"
#include "chrome/browser/chromeos/first_run/steps/app_list_step.h"
#include "chrome/browser/chromeos/first_run/steps/greeting_step.h"
#include "chrome/browser/chromeos/first_run/steps/help_step.h"
#include "chrome/browser/chromeos/first_run/steps/tray_step.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/views/widget/widget.h"

namespace {

size_t NONE_STEP_INDEX = std::numeric_limits<size_t>::max();

}  // namespace

namespace chromeos {

FirstRunController::FirstRunController()
    : actor_(NULL),
      current_step_index_(NONE_STEP_INDEX) {
}

FirstRunController::~FirstRunController() {
  if (shell_helper_.get())
    Stop();
}

void FirstRunController::Start() {
  shell_helper_.reset(ash::Shell::GetInstance()->CreateFirstRunHelper());
  FirstRunView* view = new FirstRunView();
  view->Init(ProfileManager::GetDefaultProfile());
  shell_helper_->GetOverlayWidget()->SetContentsView(view);
  actor_ = view->GetActor();
  actor_->set_delegate(this);
  if (actor_->IsInitialized())
    OnActorInitialized();
}

void FirstRunController::Stop() {
  if (GetCurrentStep())
    GetCurrentStep()->OnBeforeHide();
  steps_.clear();
  if (actor_)
    actor_->set_delegate(NULL);
  actor_ = NULL;
  shell_helper_.reset();
}

void FirstRunController::OnActorInitialized() {
  RegisterSteps();
  shell_helper_->GetOverlayWidget()->Show();
  ShowNextStep();
}

void FirstRunController::OnNextButtonClicked(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
  ShowNextStep();
}

void FirstRunController::OnActorDestroyed() {
  // Called when overlay window was destroyed not by us.
  NOTIMPLEMENTED();
}

void FirstRunController::RegisterSteps() {
  steps_.push_back(make_linked_ptr(
      new first_run::GreetingStep(shell_helper_.get(), actor_)));
  steps_.push_back(make_linked_ptr(
      new first_run::AppListStep(shell_helper_.get(), actor_)));
  steps_.push_back(make_linked_ptr(
      new first_run::TrayStep(shell_helper_.get(), actor_)));
  steps_.push_back(make_linked_ptr(
      new first_run::HelpStep(shell_helper_.get(), actor_)));
}

void FirstRunController::ShowNextStep() {
  if (GetCurrentStep())
    GetCurrentStep()->OnBeforeHide();
  AdvanceStep();
  if (GetCurrentStep())
    GetCurrentStep()->Show();
  else
    Stop();
}

void FirstRunController::AdvanceStep() {
  if (current_step_index_ == NONE_STEP_INDEX)
    current_step_index_ = 0;
  else
    ++current_step_index_;
  if (current_step_index_ >= steps_.size())
    current_step_index_ = NONE_STEP_INDEX;
}

first_run::Step* FirstRunController::GetCurrentStep() const {
  return current_step_index_ != NONE_STEP_INDEX ?
      steps_[current_step_index_].get() : NULL;
}

}  // namespace chromeos

