// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/fill_layout_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace athena {

TEST(FillLayoutManagerTest, ChildWindowSizedCorrectly) {
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->SetBounds(gfx::Rect(10, 20, 30, 40));
  parent->SetLayoutManager(new FillLayoutManager(parent.get()));

  scoped_ptr<aura::Window> child(new aura::Window(NULL));
  child->SetBounds(gfx::Rect(0, 0, 5, 10));

  EXPECT_NE(child->bounds().size().ToString(),
            parent->bounds().size().ToString());

  parent->AddChild(child.get());
  EXPECT_EQ(child->bounds().size().ToString(),
            parent->bounds().size().ToString());

  parent->SetBounds(gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ(child->bounds().size().ToString(),
            parent->bounds().size().ToString());
}

}  // namespace athena
