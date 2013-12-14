// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_controller.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/first_run/first_run_view.h"
#include "chrome/browser/chromeos/first_run/steps/app_list_step.h"
#include "chrome/browser/chromeos/first_run/steps/help_step.h"
#include "chrome/browser/chromeos/first_run/steps/tray_step.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "ui/views/widget/widget.h"

namespace {

size_t NONE_STEP_INDEX = std::numeric_limits<size_t>::max();

// Instance of currently running controller, or NULL if controller is not
// running now.
chromeos::FirstRunController* g_instance;

}  // namespace

namespace chromeos {

FirstRunController::~FirstRunController() {}

// static
void FirstRunController::Start() {
  if (g_instance) {
    LOG(WARNING) << "First-run tutorial is running already.";
    return;
  }
  g_instance = new FirstRunController();
  g_instance->Init();
}

// static
void FirstRunController::Stop() {
  if (!g_instance) {
    LOG(WARNING) << "First-run tutorial is not running.";
    return;
  }
  g_instance->Finalize();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, g_instance);
  g_instance = NULL;
}

FirstRunController::FirstRunController()
    : actor_(NULL),
      current_step_index_(NONE_STEP_INDEX),
      user_profile_(NULL) {
}

void FirstRunController::Init() {
  UserManager* user_manager = UserManager::Get();
  user_profile_ = user_manager->GetProfileByUser(user_manager->GetActiveUser());

  shell_helper_.reset(ash::Shell::GetInstance()->CreateFirstRunHelper());
  shell_helper_->AddObserver(this);

  FirstRunView* view = new FirstRunView();
  view->Init(user_profile_);
  shell_helper_->GetOverlayWidget()->SetContentsView(view);
  actor_ = view->GetActor();
  actor_->set_delegate(this);
  shell_helper_->GetOverlayWidget()->Show();
  view->RequestFocus();

  if (actor_->IsInitialized())
    OnActorInitialized();
}

void FirstRunController::Finalize() {
  if (GetCurrentStep())
    GetCurrentStep()->OnBeforeHide();
  steps_.clear();
  if (actor_)
    actor_->set_delegate(NULL);
  actor_ = NULL;
  shell_helper_->RemoveObserver(this);
  shell_helper_.reset();
}

void FirstRunController::OnActorInitialized() {
  RegisterSteps();
  ShowNextStep();
}

void FirstRunController::OnNextButtonClicked(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
  GetCurrentStep()->OnBeforeHide();
  actor_->HideCurrentStep();
}

void FirstRunController::OnHelpButtonClicked() {
  on_actor_finalized_ = base::Bind(chrome::ShowHelpForProfile,
                                   user_profile_,
                                   chrome::HOST_DESKTOP_TYPE_ASH,
                                   chrome::HELP_SOURCE_MENU);
  actor_->Finalize();
}

void FirstRunController::OnStepHidden(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
  GetCurrentStep()->OnAfterHide();
  if (!actor_->IsFinalizing())
    ShowNextStep();
}

void FirstRunController::OnActorFinalized() {
  if (!on_actor_finalized_.is_null())
    on_actor_finalized_.Run();
  Stop();
}

void FirstRunController::OnActorDestroyed() {
  // Normally this shouldn't happen because we are implicitly controlling
  // actor's lifetime.
  NOTREACHED() <<
    "FirstRunActor destroyed before FirstRunController::Finalize.";
}

void FirstRunController::OnCancelled() {
  Stop();
}

void FirstRunController::RegisterSteps() {
  steps_.push_back(make_linked_ptr(
      new first_run::AppListStep(shell_helper_.get(), actor_)));
  steps_.push_back(make_linked_ptr(
      new first_run::TrayStep(shell_helper_.get(), actor_)));
  steps_.push_back(make_linked_ptr(
      new first_run::HelpStep(shell_helper_.get(), actor_)));
}

void FirstRunController::ShowNextStep() {
  AdvanceStep();
  if (GetCurrentStep())
    GetCurrentStep()->Show();
  else
    actor_->Finalize();
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

