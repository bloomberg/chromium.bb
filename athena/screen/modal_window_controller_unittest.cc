// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/modal_window_controller.h"
#include "athena/screen/screen_manager_impl.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/test/base/test_windows.h"
#include "athena/util/container_priorities.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace athena {

typedef test::AthenaTestBase ModalWindowControllerTest;

aura::Window* FindContainerByPriority(int priority) {
  ScreenManagerImpl* screen_manager =
      static_cast<ScreenManagerImpl*>(ScreenManager::Get());
  return screen_manager->FindContainerByPriority(priority);
}

TEST_F(ModalWindowControllerTest, ModalContainer) {
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> modal(test::CreateTransientWindow(
      &delegate, nullptr, ui::MODAL_TYPE_SYSTEM, false));

  aura::Window* modal_container = FindContainerByPriority(CP_SYSTEM_MODAL);
  EXPECT_TRUE(modal_container);

  ModalWindowController* modal_controller =
      ModalWindowController::Get(modal_container);
  ASSERT_TRUE(modal_controller);
  EXPECT_EQ(modal_container, modal->parent());
  EXPECT_FALSE(modal_controller->dimmed());
  modal->Show();
  EXPECT_TRUE(modal_controller->dimmed());

  modal->Hide();
  EXPECT_TRUE(FindContainerByPriority(CP_SYSTEM_MODAL));
  EXPECT_FALSE(modal_controller->dimmed());

  modal->Show();
  EXPECT_TRUE(FindContainerByPriority(CP_SYSTEM_MODAL));
  EXPECT_TRUE(modal_controller->dimmed());

  modal.reset();
  EXPECT_FALSE(FindContainerByPriority(CP_SYSTEM_MODAL));

  // Create two.
  modal = test::CreateTransientWindow(
              &delegate, nullptr, ui::MODAL_TYPE_SYSTEM, false).Pass();
  scoped_ptr<aura::Window> modal2(test::CreateTransientWindow(
      &delegate, nullptr, ui::MODAL_TYPE_SYSTEM, false));

  modal_container = FindContainerByPriority(CP_SYSTEM_MODAL);
  EXPECT_TRUE(modal_container);
  modal_controller = ModalWindowController::Get(modal_container);
  ASSERT_TRUE(modal_controller);
  EXPECT_EQ(modal_container, modal->parent());
  EXPECT_EQ(modal_container, modal2->parent());

  EXPECT_FALSE(modal_controller->dimmed());
  modal->Show();
  EXPECT_TRUE(modal_controller->dimmed());
  modal2->Show();
  EXPECT_TRUE(modal_controller->dimmed());

  modal->Hide();
  EXPECT_TRUE(modal_controller->dimmed());
  modal2->Hide();
  EXPECT_FALSE(modal_controller->dimmed());
  EXPECT_TRUE(FindContainerByPriority(CP_SYSTEM_MODAL));

  modal2.reset();
  EXPECT_TRUE(FindContainerByPriority(CP_SYSTEM_MODAL));
  EXPECT_FALSE(modal_controller->dimmed());

  modal.reset();
  EXPECT_FALSE(FindContainerByPriority(CP_SYSTEM_MODAL));
}

TEST_F(ModalWindowControllerTest, NestedModalWindows) {
  ScreenManager::ContainerParams params("top", CP_END);
  params.can_activate_children = true;
  params.modal_container_priority = CP_END + 1;
  aura::Window* top_container = ScreenManager::Get()->CreateContainer(params);

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> top_w1(
      test::CreateNormalWindow(&delegate, top_container));
  EXPECT_EQ(top_container, top_w1->parent());

  aura::Window* default_container = FindContainerByPriority(CP_DEFAULT);
  EXPECT_TRUE(default_container);

  scoped_ptr<aura::Window> normal_w1(
      test::CreateNormalWindow(&delegate, nullptr));
  EXPECT_EQ(default_container, normal_w1->parent());

  scoped_ptr<aura::Window> normal_m1(test::CreateTransientWindow(
      &delegate, normal_w1.get(), ui::MODAL_TYPE_SYSTEM, false));
  aura::Window* default_modal_container =
      FindContainerByPriority(CP_SYSTEM_MODAL);
  EXPECT_EQ(default_modal_container, normal_m1->parent());

  // A modal window with the transient parent which doesn't specify
  // the modal container should fallback the default container's modal
  // container.
  aura::Window* home_container = FindContainerByPriority(CP_DEFAULT);
  EXPECT_TRUE(home_container);

  scoped_ptr<aura::Window> normal_m2(test::CreateTransientWindow(
      &delegate, home_container, ui::MODAL_TYPE_SYSTEM, false));
  EXPECT_EQ(default_modal_container, normal_m2->parent());

  // No modal container for top container yet.
  EXPECT_FALSE(FindContainerByPriority(CP_END + 1));

  // Creating a modal with always on top creates the modal dialog on top
  // most dialog container.
  scoped_ptr<aura::Window> top_m0(test::CreateTransientWindow(
      &delegate, nullptr, ui::MODAL_TYPE_SYSTEM, true));

  aura::Window* top_modal_container = FindContainerByPriority(CP_END + 1);
  EXPECT_EQ(top_modal_container, top_m0->parent());

  // Creating a modal dialog with transient parent on the top container
  // creates the winodw on the top container's modal container.
  scoped_ptr<aura::Window> top_m1(test::CreateTransientWindow(
      &delegate, top_w1.get(), ui::MODAL_TYPE_SYSTEM, true));
  EXPECT_EQ(top_modal_container, top_m1->parent());

  normal_m1.reset();
  EXPECT_TRUE(FindContainerByPriority(CP_SYSTEM_MODAL));
  normal_m2.reset();
  EXPECT_FALSE(FindContainerByPriority(CP_SYSTEM_MODAL));

  top_m0.reset();
  EXPECT_TRUE(FindContainerByPriority(CP_END + 1));
  top_m1.reset();
  EXPECT_FALSE(FindContainerByPriority(CP_END + 1));
}

}  // namespace athena
