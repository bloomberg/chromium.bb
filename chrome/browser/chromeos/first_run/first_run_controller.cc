// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_controller.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/first_run/first_run_view.h"
#include "chrome/browser/chromeos/first_run/metrics.h"
#include "chrome/browser/chromeos/first_run/steps/app_list_step.h"
#include "chrome/browser/chromeos/first_run/steps/help_step.h"
#include "chrome/browser/chromeos/first_run/steps/tray_step.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "ui/views/widget/widget.h"

namespace {

size_t NONE_STEP_INDEX = std::numeric_limits<size_t>::max();

// Instance of currently running controller, or NULL if controller is not
// running now.
chromeos::FirstRunController* g_instance;

void RecordCompletion(chromeos::first_run::TutorialCompletion type) {
  UMA_HISTOGRAM_ENUMERATION("CrosFirstRun.TutorialCompletion",
                            type,
                            chromeos::first_run::TUTORIAL_COMPLETION_SIZE);
}

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

FirstRunController* FirstRunController::GetInstanceForTest() {
  return g_instance;
}

FirstRunController::FirstRunController()
    : actor_(NULL),
      current_step_index_(NONE_STEP_INDEX),
      user_profile_(NULL) {
}

void FirstRunController::Init() {
  start_time_ = base::Time::Now();
  UserManager* user_manager = UserManager::Get();
  user_profile_ = ProfileHelper::Get()->GetProfileByUserUnsafe(
      user_manager->GetActiveUser());

  shell_helper_.reset(ash::Shell::GetInstance()->CreateFirstRunHelper());
  shell_helper_->AddObserver(this);

  FirstRunView* view = new FirstRunView();
  view->Init(user_profile_);
  shell_helper_->GetOverlayWidget()->SetContentsView(view);
  actor_ = view->GetActor();
  actor_->set_delegate(this);
  shell_helper_->GetOverlayWidget()->Show();
  view->RequestFocus();
  web_contents_for_tests_ = view->GetWebContents();

  if (actor_->IsInitialized())
    OnActorInitialized();
}

void FirstRunController::Finalize() {
  int furthest_step = current_step_index_ == NONE_STEP_INDEX
                          ? steps_.size() - 1
                          : current_step_index_;
  UMA_HISTOGRAM_ENUMERATION("CrosFirstRun.FurthestStep",
                            furthest_step,
                            steps_.size());
  UMA_HISTOGRAM_MEDIUM_TIMES("CrosFirstRun.TimeSpent",
                             base::Time::Now() - start_time_);
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
  RecordCompletion(first_run::TUTORIAL_COMPLETED_WITH_KEEP_EXPLORING);
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

void FirstRunController::OnStepShown(const std::string& step_name) {
  DCHECK(GetCurrentStep() && GetCurrentStep()->name() == step_name);
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
  RecordCompletion(first_run::TUTORIAL_NOT_FINISHED);
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
  if (!GetCurrentStep()) {
    actor_->Finalize();
    RecordCompletion(first_run::TUTORIAL_COMPLETED_WITH_GOT_IT);
    return;
  }
  GetCurrentStep()->Show();
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

