// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "athena/screen/screen_manager_impl.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/test/base/test_windows.h"
#include "athena/util/container_priorities.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

using ScreenManagerTest = athena::test::AthenaTestBase;
using AthenaFocusRuleTest = athena::test::AthenaTestBase;

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

  scoped_ptr<aura::Window> window(CreateWindow(
      no_activatable_container, nullptr, gfx::Rect(0, 0, 100, 100)));
  EXPECT_FALSE(wm::CanActivateWindow(window.get()));

  activatable_container->AddChild(window.get());
  EXPECT_TRUE(wm::CanActivateWindow(window.get()));
}

TEST_F(ScreenManagerTest, BlockInputsShouldNotBlockVirtualKeyboard) {
  ScreenManager::ContainerParams block_params("blocking", kTestZOrderPriority);
  block_params.can_activate_children = true;
  block_params.block_events = true;
  aura::Window* block_container =
      ScreenManager::Get()->CreateContainer(block_params);

  aura::test::EventCountDelegate block_delegate;
  scoped_ptr<aura::Window> block_window(CreateWindow(
      block_container, &block_delegate, gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(wm::CanActivateWindow(block_window.get()));

  // Create a normal container appearing over the |block_container|. This is
  // essentially the case of virtual keyboard.
  ScreenManager::ContainerParams vk_params("virtual keyboard",
                                           kTestZOrderPriority + 1);
  vk_params.can_activate_children = true;
  aura::Window* vk_container = ScreenManager::Get()->CreateContainer(vk_params);

  aura::test::EventCountDelegate vk_delegate;
  scoped_ptr<aura::Window> vk_window(
      CreateWindow(vk_container, &vk_delegate, gfx::Rect(0, 20, 100, 80)));
  EXPECT_TRUE(wm::CanActivateWindow(vk_window.get()));

  ui::test::EventGenerator event_generator(root_window());
  event_generator.MoveMouseTo(10, 25);
  event_generator.ClickLeftButton();
  EXPECT_EQ("0 0", block_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("1 1", vk_delegate.GetMouseButtonCountsAndReset());
}

TEST_F(ScreenManagerTest, DefaultContainer) {
  ScreenManagerImpl* impl =
      static_cast<ScreenManagerImpl*>(ScreenManager::Get());
  aura::Window* original_default = impl->FindContainerByPriority(CP_DEFAULT);
  aura::Window* parent = original_default->parent();
  // Temporarily remove the original default container from tree.
  parent->RemoveChild(original_default);

  ScreenManager::ContainerParams params("new_default", CP_END + 1);
  params.default_parent = true;
  params.modal_container_priority = CP_END + 2;
  aura::Window* new_default = ScreenManager::Get()->CreateContainer(params);
  aura::Window* w = test::CreateNormalWindow(nullptr, nullptr).release();
  EXPECT_EQ(new_default, w->parent());
  delete new_default;

  // Add the original back to shutdown properly.
  parent->AddChild(original_default);
}

TEST_F(AthenaFocusRuleTest, FocusTravarsalFromSameContainer) {
  ScreenManager::ContainerParams params("contaier", kTestZOrderPriority);
  params.can_activate_children = true;
  scoped_ptr<aura::Window>
      container(ScreenManager::Get()->CreateContainer(params));

  scoped_ptr<aura::Window> w1(CreateWindow(
      container.get(), nullptr, gfx::Rect(0, 0, 100, 100)));
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  scoped_ptr<aura::Window> w2(CreateWindow(
      container.get(), nullptr, gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  container->RemoveChild(w1.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

TEST_F(AthenaFocusRuleTest, FocusTravarsalFromOtherContainer) {
  ScreenManager::ContainerParams params2("contaier2", kTestZOrderPriority + 1);
  params2.can_activate_children = true;
  scoped_ptr<aura::Window>
      container2(ScreenManager::Get()->CreateContainer(params2));
  scoped_ptr<aura::Window> w2(CreateWindow(
      container2.get(), nullptr, gfx::Rect(0, 0, 100, 100)));
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  ScreenManager::ContainerParams params1("contaier1", kTestZOrderPriority);
  params1.can_activate_children = true;
  scoped_ptr<aura::Window>
      container1(ScreenManager::Get()->CreateContainer(params1));
  ScreenManager::ContainerParams params3("contaier3", kTestZOrderPriority + 2);
  params3.can_activate_children = true;
  scoped_ptr<aura::Window>
      container3(ScreenManager::Get()->CreateContainer(params3));
  scoped_ptr<aura::Window> w1(CreateWindow(
      container1.get(), nullptr, gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> w3(CreateWindow(
      container3.get(), nullptr, gfx::Rect(0, 0, 100, 100)));

  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  container2->RemoveChild(w2.get());
  // Focus moves to a window in the front contaier.
  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  container3->RemoveChild(w3.get());
  // Focus moves to a window in the back contaier.
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

TEST_F(AthenaFocusRuleTest, FocusTravarsalFromEventBlockedContainer) {
  ScreenManager::ContainerParams params1("contaier1", kTestZOrderPriority + 1);
  params1.can_activate_children = true;
  scoped_ptr<aura::Window>
      container1(ScreenManager::Get()->CreateContainer(params1));

  ScreenManager::ContainerParams params2("contaier2", kTestZOrderPriority + 2);
  params2.can_activate_children = true;
  params2.block_events = true;
  scoped_ptr<aura::Window>
      container2(ScreenManager::Get()->CreateContainer(params2));

  scoped_ptr<aura::Window> w1(CreateWindow(
      container1.get(), nullptr, gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> w2(CreateWindow(
      container2.get(), nullptr, gfx::Rect(0, 0, 100, 100)));

  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Confirm that w1 can't get the focus.
  wm::ActivateWindow(w1.get());
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  container2->Hide();
  w2.reset();
  container2.reset();

  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

namespace {

class ScreenManagerTargeterTest
    : public athena::test::AthenaTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ScreenManagerTargeterTest()
      : targeter_(GetParam() ? nullptr : new aura::WindowTargeter) {}
  ~ScreenManagerTargeterTest() override {}

 protected:
  scoped_ptr<ui::EventTargeter> targeter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenManagerTargeterTest);
};

}  // namespace

TEST_P(ScreenManagerTargeterTest, BlockContainer) {
  ScreenManager::ContainerParams normal_params(
      "normal", kTestZOrderPriority);
  normal_params.can_activate_children = true;
  aura::Window* normal_container =
      ScreenManager::Get()->CreateContainer(normal_params);
  normal_container->SetEventTargeter(targeter_.Pass());

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

  ScreenManager::ContainerParams block_params("blocking",
                                              kTestZOrderPriority + 1);
  block_params.can_activate_children = true;
  block_params.block_events = true;
  aura::Window* block_container =
      ScreenManager::Get()->CreateContainer(block_params);

  EXPECT_FALSE(wm::CanActivateWindow(normal_window.get()));

  aura::test::EventCountDelegate block_delegate;
  scoped_ptr<aura::Window> block_window(CreateWindow(
      block_container, &block_delegate, gfx::Rect(10, 10, 100, 100)));
  EXPECT_TRUE(wm::CanActivateWindow(block_window.get()));

  wm::ActivateWindow(block_window.get());

  // (0, 0) is still on normal_window, but the event should not go there
  // because blockbing_container prevents it.
  event_generator.MoveMouseTo(0, 0);
  event_generator.ClickLeftButton();
  EXPECT_EQ("0 0", normal_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0", block_delegate.GetMouseButtonCountsAndReset());
  event_generator.MoveMouseWheel(0, 10);
  // EXPECT_EQ(0, normal_event_counter.num_scroll_events());

  event_generator.MoveMouseTo(20, 20);
  event_generator.ClickLeftButton();
  EXPECT_EQ("1 1", block_delegate.GetMouseButtonCountsAndReset());

  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator.ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ("0 0", normal_delegate.GetKeyCountsAndReset());
  EXPECT_EQ("1 1", block_delegate.GetKeyCountsAndReset());
}

TEST_P(ScreenManagerTargeterTest, BlockAndMouseCapture) {
  ScreenManager::ContainerParams normal_params(
      "normal", kTestZOrderPriority);
  normal_params.can_activate_children = true;
  aura::Window* normal_container =
      ScreenManager::Get()->CreateContainer(normal_params);
  normal_container->SetEventTargeter(targeter_.Pass());

  aura::test::EventCountDelegate normal_delegate;
  scoped_ptr<aura::Window> normal_window(CreateWindow(
      normal_container, &normal_delegate, gfx::Rect(0, 0, 100, 100)));

  ui::test::EventGenerator event_generator(root_window());
  event_generator.MoveMouseTo(0, 0);
  event_generator.PressLeftButton();

  // Creating blocking container while mouse pressing.
  ScreenManager::ContainerParams block_params("blocking",
                                              kTestZOrderPriority + 1);
  block_params.can_activate_children = true;
  block_params.block_events = true;
  aura::Window* block_container =
      ScreenManager::Get()->CreateContainer(block_params);

  aura::test::EventCountDelegate block_delegate;
  scoped_ptr<aura::Window> block_window(CreateWindow(
      block_container, &block_delegate, gfx::Rect(10, 10, 100, 100)));

  // Release event should be sent to |normal_window| because it captures the
  // mouse event.
  event_generator.ReleaseLeftButton();
  EXPECT_EQ("1 1", normal_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0", block_delegate.GetMouseButtonCountsAndReset());

  // After release, further mouse events should not be sent to |normal_window|
  // because block_container blocks the input.
  event_generator.ClickLeftButton();
  EXPECT_EQ("0 0", normal_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ("0 0", block_delegate.GetMouseButtonCountsAndReset());
}

INSTANTIATE_TEST_CASE_P(WithOrWithoutTargeter,
                        ScreenManagerTargeterTest,
                        testing::Values(false, true));

}  // namespace athena
