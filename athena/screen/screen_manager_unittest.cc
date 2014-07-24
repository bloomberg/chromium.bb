// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"

#include <string>

#include "athena/test/athena_test_base.h"
#include "ui/aura/window.h"

typedef athena::test::AthenaTestBase ScreenManagerTest;

namespace athena {
namespace {

aura::Window* Create(const std::string& name, int z_order_priority) {
  ScreenManager::ContainerParams params(name, z_order_priority);
  return ScreenManager::Get()->CreateContainer(params);
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

}  // namespace athena
