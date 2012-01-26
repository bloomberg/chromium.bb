// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

using aura::Window;

class WorkspaceControllerTest : public aura::test::AuraTestBase {
 public:
  WorkspaceControllerTest() {}
  virtual ~WorkspaceControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    contents_view_ = aura::RootWindow::GetInstance();
    // Activatable windows need to be in a container the ActivationController
    // recognizes.
    contents_view_->set_id(
        ash::internal::kShellWindowId_DefaultContainer);
    activation_controller_.reset(new ActivationController);
    activation_controller_->set_default_container_for_test(contents_view_);
    controller_.reset(new WorkspaceController(contents_view_));
  }

  virtual void TearDown() OVERRIDE {
    activation_controller_.reset();
    controller_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    contents_view()->AddChild(window);
    window->Show();
    return window;
  }

  aura::Window* contents_view() {
    return contents_view_;
  }

  WorkspaceManager* workspace_manager() {
    return controller_->workspace_manager();
  }

  scoped_ptr<WorkspaceController> controller_;

 private:
  aura::Window* contents_view_;

  scoped_ptr<ActivationController> activation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceControllerTest);
};

}  // namespace internal
}  // namespace ash
