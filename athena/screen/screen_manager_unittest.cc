// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"

#include <algorithm>
#include <string>

#include "athena/test/athena_test_base.h"
#include "athena/util/container_priorities.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

typedef athena::test::AthenaTestBase ScreenManagerTest;

namespace athena {
namespace {

const int kTestZOrderPriority = 10;

aura::Window* Create(const std::string& name, int z_order_priority) {
  ScreenManager::ContainerParams params(name, z_order_priority);
  return ScreenManager::Get()->CreateContainer(params);
}

aura::Window* CreateWindow(aura::Window* container,
                           aura::WindowDelegate* delegate,
                           const gfx::Rect& bounds) {
  aura::Window* window = new aura::Window(delegate);
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  container->AddChild(window);
  window->Show();
  window->SetBounds(bounds);
  return window;
}

void CheckZOrder(aura::Window* w1, aura::Window* w2) {
  aura::Window* parent = w1->parent();
  const aura::Window::Windows& children = parent->children();
  aura::Window::Windows::const_iterator begin_iter = children.begin();
  aura::Window::Windows::const_iterator end_iter = children.end();

  aura::Window::Windows::const_iterator w1_iter =
      std::find(begin_iter, end_iter, w1);
  aura::Window::Windows::const_iterator w2_iter =
      std::find(begin_iter, end_iter, w2);
  EXPECT_NE(end_iter, w1_iter);
  EXPECT_NE(end_iter, w2_iter);
  EXPECT_TRUE(w1_iter < w2_iter);
}

}  // namespace

TEST_F(ScreenManagerTest, CreateContainer) {
  size_t num_containers = root_window()->children().size();

  aura::Window* container = Create("test", kTestZOrderPriority);
  EXPECT_EQ("test", container->name());

  const aura::Window::Windows& containers = root_window()->children();
  EXPECT_EQ(num_containers + 1, containers.size());
  EXPECT_NE(containers.end(),
            std::find(containers.begin(), containers.end(), container));
}

TEST_F(ScreenManagerTest, Zorder) {
  aura::Window* window_10 = Create("test10", 10);
  aura::Window* window_11 = Create("test11", 11);
  aura::Window* window_12 = Create("test12", 12);

  {
    SCOPED_TRACE("Init");
    CheckZOrder(window_10, window_11);
    CheckZOrder(window_11, window_12);
  }
  {
    SCOPED_TRACE("Delete");
    delete window_11;
    CheckZOrder(window_10, window_12);
  }
  {
    SCOPED_TRACE("Insert");
    window_11 = Create("test11", 11);
    CheckZOrder(window_10, window_11);
    CheckZOrder(window_11, window_12);
  }
}

TEST_F(ScreenManagerTest, NonActivatableContainer) {
  ScreenManager::ContainerParams non_activatable(
      "non_activatable", kTestZOrderPriority);
  non_activatable.can_activate_children = false;
  aura::Window* no_activatable_container =
      ScreenManager::Get()->CreateContainer(non_activatable);

  ScreenManager::ContainerParams activatable(
      "activatable", kTestZOrderPriority + 1);
  activatable.can_activate_children = true;
  aura::Window* activatable_container =
      ScreenManager::Get()->CreateContainer(activatable);

  scoped_ptr<aura::Window> window(
      CreateWindow(no_activatable_container, NULL, gfx::Rect(0, 0, 100, 100)));
  EXPECT_FALSE(wm::CanActivateWindow(window.get()));

  activatable_container->AddChild(window.get());
  EXPECT_TRUE(wm::CanActivateWindow(window.get()));
}

TEST_F(ScreenManagerTest, GrabInputContainer) {
  ScreenManager::ContainerParams normal_params(
      "normal", kTestZOrderPriority);
  normal_params.can_activate_children = true;
  aura::Window* normal_container =
      ScreenManager::Get()->CreateContainer(normal_params);

  aura::test::EventCountDelegate normal_delegate;
  scoped_ptr<aura::Window> normal_window(CreateWindow(
      normal_container, &normal_delegate, gfx::Rect(0, 0, 100, 100)));

  EXPECT_TRUE(wm::CanActivateWindow(normal_window.get()));
  wm::ActivateWindow(normal_window.get());
  ui::test::EventGenerator event_generator(root_window());
  event_generator.MoveMouseTo(0, 0);
  event_generator.ClickLeftButton();
  EXPECT_EQ("1 1", normal_delegate.GetMouseButtonCountsAndReset());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator.ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ("1 1", normal_delegate.GetKeyCountsAndReset());

  ScreenManager::ContainerParams grab_params(
      "grabbing", kTestZOrderPriority + 1);
  grab_params.can_activate_children = true;
  grab_params.grab_inputs = true;
  aura::Window* grab_container =
      ScreenManager::Get()->CreateContainer(grab_params);

  EXPECT_FALSE(wm::CanActivateWindow(normal_window.get()));

  aura::test::EventCountDelegate grab_delegate;
  scoped_ptr<aura::Window> grab_window(CreateWindow(
      grab_container, &grab_delegate, gfx::Rect(10, 10, 100, 100)));
  EXPECT_TRUE(wm::CanActivateWindow(grab_window.get()));

  wm::ActivateWindow(grab_window.get());

  // (0, 0) is still on normal_window, but the event should not go there
  // because grabbing_container prevents it.
  event_generator.MoveMouseTo(0, 0);
  event_generator.ClickLeftButton();
  EXPECT_EQ("0 0", normal_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0", grab_delegate.GetMouseButtonCountsAndReset());

  event_generator.MoveMouseTo(20, 20);
  event_generator.ClickLeftButton();
  EXPECT_EQ("1 1", grab_delegate.GetMouseButtonCountsAndReset());

  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator.ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ("0 0", normal_delegate.GetKeyCountsAndReset());
  EXPECT_EQ("1 1", grab_delegate.GetKeyCountsAndReset());
}

TEST_F(ScreenManagerTest, GrabShouldNotBlockVirtualKeyboard) {
  ScreenManager::ContainerParams grab_params("grabbing", kTestZOrderPriority);
  grab_params.can_activate_children = true;
  grab_params.grab_inputs = true;
  aura::Window* grab_container =
      ScreenManager::Get()->CreateContainer(grab_params);

  aura::test::EventCountDelegate grab_delegate;
  scoped_ptr<aura::Window> grab_window(
      CreateWindow(grab_container, &grab_delegate, gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(wm::CanActivateWindow(grab_window.get()));

  // Create a normal container appearing over the |grab_container|. This is
  // essentially the case of virtual keyboard.
  ScreenManager::ContainerParams vk_params(
      "virtual keyboard", kTestZOrderPriority + 1);
  vk_params.can_activate_children = true;
  aura::Window* vk_container = ScreenManager::Get()->CreateContainer(vk_params);

  aura::test::EventCountDelegate vk_delegate;
  scoped_ptr<aura::Window> vk_window(
      CreateWindow(vk_container, &vk_delegate, gfx::Rect(0, 20, 100, 80)));
  EXPECT_TRUE(wm::CanActivateWindow(vk_window.get()));

  ui::test::EventGenerator event_generator(root_window());
  event_generator.MoveMouseTo(10, 25);
  event_generator.ClickLeftButton();
  EXPECT_EQ("0 0", grab_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("1 1", vk_delegate.GetMouseButtonCountsAndReset());
}

TEST_F(ScreenManagerTest, GrabAndMouseCapture) {
  ScreenManager::ContainerParams normal_params(
      "normal", kTestZOrderPriority);
  normal_params.can_activate_children = true;
  aura::Window* normal_container =
      ScreenManager::Get()->CreateContainer(normal_params);

  aura::test::EventCountDelegate normal_delegate;
  scoped_ptr<aura::Window> normal_window(CreateWindow(
      normal_container, &normal_delegate, gfx::Rect(0, 0, 100, 100)));

  ui::test::EventGenerator event_generator(root_window());
  event_generator.MoveMouseTo(0, 0);
  event_generator.PressLeftButton();

  // Creating grabbing container while mouse pressing.
  ScreenManager::ContainerParams grab_params(
      "grabbing", kTestZOrderPriority + 1);
  grab_params.can_activate_children = true;
  grab_params.grab_inputs = true;
  aura::Window* grab_container =
      ScreenManager::Get()->CreateContainer(grab_params);

  aura::test::EventCountDelegate grab_delegate;
  scoped_ptr<aura::Window> grab_window(CreateWindow(
      grab_container, &grab_delegate, gfx::Rect(10, 10, 100, 100)));

  // Release event should be sent to |normal_window| because it captures the
  // mouse event.
  event_generator.ReleaseLeftButton();
  EXPECT_EQ("1 1", normal_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0", grab_delegate.GetMouseButtonCountsAndReset());

  // After release, further mouse events should not be sent to |normal_window|
  // because grab_container grabs the input.
  event_generator.ClickLeftButton();
  EXPECT_EQ("0 0", normal_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0", grab_delegate.GetMouseButtonCountsAndReset());
}

}  // namespace athena
