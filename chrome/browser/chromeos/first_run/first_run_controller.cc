// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/first_run_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/first_run/first_run_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/views/widget/widget.h"

namespace {

gfx::Rect GetScreenBounds() {
  return ash::Shell::GetScreen()->GetPrimaryDisplay().bounds();
}

views::Widget* CreateFirstRunWindow() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = GetScreenBounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(),
          ash::internal::kShellWindowId_OverlayContainer);
  views::Widget* window = new views::Widget;
  window->Init(params);
  return window;
}

// We can't get launcher size now in normal way. This workaround uses fact that
// AppList button is the rightmost button in Launcher.
gfx::Rect GetLauncherBounds() {
  ash::Launcher* launcher = ash::Launcher::ForPrimaryDisplay();
  views::View* app_button = launcher->GetAppListButtonView();
  gfx::Rect bounds = app_button->GetBoundsInScreen();
  return gfx::Rect(0, bounds.y(), bounds.right(), bounds.height());
}

}  // anonymous namespace

namespace chromeos {

FirstRunController::FirstRunController()
    : window_(NULL),
      actor_(NULL) {
}

FirstRunController::~FirstRunController() {
  if (window_)
    Stop();
}

void FirstRunController::Start() {
  if (window_)
    return;
  window_ = CreateFirstRunWindow();
  FirstRunView* view = new FirstRunView();
  view->Init(ProfileManager::GetDefaultProfile());
  window_->SetContentsView(view);
  actor_ = view->GetActor();
  actor_->set_delegate(this);
  if (actor_->IsInitialized())
    OnActorInitialized();
}

void FirstRunController::Stop() {
  window_->Close();
  window_ = NULL;
  if (actor_)
    actor_->set_delegate(NULL);
  actor_ = NULL;
}

void FirstRunController::OnActorInitialized() {
  window_->Show();
  gfx::Rect launcher_bounds = GetLauncherBounds();
  actor_->AddBackgroundHole(launcher_bounds.x(),
                            launcher_bounds.y(),
                            launcher_bounds.width(),
                            launcher_bounds.height());
  actor_->ShowStep("first-step",
                   FirstRunActor::StepPosition()
                       .SetLeft(0)
                       .SetBottom(
                           GetScreenBounds().height() - launcher_bounds.y()));
}

void FirstRunController::OnNextButtonClicked(const std::string& step_name) {
  CHECK(step_name == "first-step");
  Stop();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void FirstRunController::OnActorDestroyed() {
  actor_ = NULL;
}

}  // namespace chromeos

