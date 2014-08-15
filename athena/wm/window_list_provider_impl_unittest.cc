// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_list_provider_impl.h"

#include <algorithm>

#include "athena/test/athena_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace athena {

namespace {

bool AreWindowListsEqual(const aura::Window::Windows& one,
                         const aura::Window::Windows& two) {
  return one.size() == two.size() &&
         std::equal(one.begin(), one.end(), two.begin());
}

scoped_ptr<aura::Window> CreateWindow(aura::WindowDelegate* delegate,
                                      ui::wm::WindowType window_type) {
  scoped_ptr<aura::Window> window(new aura::Window(delegate));
  window->SetType(window_type);
  window->Init(aura::WINDOW_LAYER_SOLID_COLOR);
  return window.Pass();
}

}  // namespace

typedef test::AthenaTestBase WindowListProviderImplTest;

// Tests that the order of windows match the stacking order of the windows in
// the container, even after the order is changed through the aura Window API.
TEST_F(WindowListProviderImplTest, StackingOrder) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<aura::Window> first =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> second =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> third =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_NORMAL);
  container->AddChild(first.get());
  container->AddChild(second.get());
  container->AddChild(third.get());

  scoped_ptr<WindowListProvider> list_provider(
      new WindowListProviderImpl(container.get()));
  EXPECT_TRUE(AreWindowListsEqual(container->children(),
                                  list_provider->GetWindowList()));

  container->StackChildAtTop(first.get());
  EXPECT_TRUE(AreWindowListsEqual(container->children(),
                                  list_provider->GetWindowList()));
  EXPECT_EQ(first.get(), container->children().back());
}

TEST_F(WindowListProviderImplTest, ListContainsOnlyNormalWindows) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<aura::Window> first =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> second =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_POPUP);
  scoped_ptr<aura::Window> third =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> fourth =
      CreateWindow(&delegate, ui::wm::WINDOW_TYPE_MENU);
  container->AddChild(first.get());
  container->AddChild(second.get());
  container->AddChild(third.get());
  container->AddChild(fourth.get());

  scoped_ptr<WindowListProvider> list_provider(
      new WindowListProviderImpl(container.get()));
  const aura::Window::Windows list = list_provider->GetWindowList();
  EXPECT_EQ(list.end(), std::find(list.begin(), list.end(), second.get()));
  EXPECT_EQ(list.end(), std::find(list.begin(), list.end(), fourth.get()));
  EXPECT_NE(list.end(), std::find(list.begin(), list.end(), first.get()));
  EXPECT_NE(list.end(), std::find(list.begin(), list.end(), third.get()));
}

}  // namespace athena
