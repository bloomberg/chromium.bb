// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

namespace ash {

class StackingControllerTest : public AshTestBase {
 public:
  StackingControllerTest() = default;
  ~StackingControllerTest() override = default;

  aura::Window* CreateTestWindow() {
    aura::Window* window =
        window_factory::NewWindow(nullptr, aura::client::WINDOW_TYPE_NORMAL)
            .release();
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    return window;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StackingControllerTest);
};

// Verifies a window with a transient parent is in the same container as its
// transient parent.
TEST_F(StackingControllerTest, TransientParent) {
  // Normal window .
  std::unique_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  aura::Window* launcher = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                               kShellWindowId_ShelfContainer);
  launcher->AddChild(w2.get());
  w2->Show();

  wm::ActivateWindow(w2.get());

  // Window with a transient parent.
  std::unique_ptr<Window> w1(CreateTestWindow());
  ::wm::AddTransientChild(w2.get(), w1.get());
  w1->SetBounds(gfx::Rect(10, 11, 250, 251));
  ParentWindowInPrimaryRootWindow(w1.get());
  w1->Show();
  wm::ActivateWindow(w1.get());

  // The window with the transient parent should get added to the same container
  // as its transient parent.
  EXPECT_EQ(launcher, w1->parent());
}

}  // namespace ash
