// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/fill_layout_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/wm/public/window_types.h"

namespace athena {

TEST(FillLayoutManagerTest, ChildWindowSizedCorrectly) {
  scoped_ptr<aura::Window> parent(new aura::Window(nullptr));
  parent->SetBounds(gfx::Rect(10, 20, 30, 40));
  parent->SetLayoutManager(new FillLayoutManager(parent.get()));

  scoped_ptr<aura::Window> child(new aura::Window(nullptr));
  child->SetBounds(gfx::Rect(0, 0, 5, 10));

  EXPECT_NE(parent->bounds().size().ToString(),
            child->bounds().size().ToString());

  parent->AddChild(child.get());
  EXPECT_EQ(parent->bounds().size().ToString(),
            child->bounds().size().ToString());

  parent->SetBounds(gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ(parent->bounds().size().ToString(),
            child->bounds().size().ToString());

  // Menu, tooltip, and popup should not be filled.
  scoped_ptr<aura::Window> menu(new aura::Window(nullptr));
  menu->SetType(ui::wm::WINDOW_TYPE_MENU);
  menu->SetBounds(gfx::Rect(0, 0, 5, 10));

  EXPECT_EQ("0,0 5x10", menu->bounds().ToString());
  parent->AddChild(menu.get());
  EXPECT_EQ("0,0 5x10", menu->bounds().ToString());
  menu->SetBounds(gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ("0,0 100x200", menu->bounds().ToString());

  scoped_ptr<aura::Window> tooltip(new aura::Window(nullptr));
  tooltip->SetType(ui::wm::WINDOW_TYPE_TOOLTIP);
  tooltip->SetBounds(gfx::Rect(0, 0, 5, 10));

  EXPECT_EQ("0,0 5x10", tooltip->bounds().ToString());
  parent->AddChild(tooltip.get());
  EXPECT_EQ("0,0 5x10", tooltip->bounds().ToString());
  tooltip->SetBounds(gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ("0,0 100x200", tooltip->bounds().ToString());

  scoped_ptr<aura::Window> popup(new aura::Window(nullptr));
  popup->SetType(ui::wm::WINDOW_TYPE_POPUP);
  popup->SetBounds(gfx::Rect(0, 0, 5, 10));

  EXPECT_EQ("0,0 5x10", popup->bounds().ToString());
  parent->AddChild(popup.get());
  EXPECT_EQ("0,0 5x10", popup->bounds().ToString());
  popup->SetBounds(gfx::Rect(0, 0, 100, 200));
  EXPECT_EQ("0,0 100x200", popup->bounds().ToString());

  // Frameless window is TYPE_POPUP, but some frameless window may want to be
  // filled with the specific key.
  scoped_ptr<aura::Window> frameless(new aura::Window(nullptr));
  frameless->SetType(ui::wm::WINDOW_TYPE_POPUP);
  frameless->SetBounds(gfx::Rect(0, 0, 5, 10));

  EXPECT_EQ("0,0 5x10", frameless->bounds().ToString());

  // Adding frameless to |parent|, then set the flag. This order respects
  // the actual order of creating a views::Widget.
  parent->AddChild(frameless.get());
  FillLayoutManager::SetAlwaysFill(frameless.get());
  frameless->Show();

  EXPECT_EQ(parent->bounds().size().ToString(),
            frameless->bounds().size().ToString());

  frameless->SetBounds(gfx::Rect(0, 0, 10, 20));
  EXPECT_EQ(parent->bounds().size().ToString(),
            frameless->bounds().size().ToString());
}

}  // namespace athena
