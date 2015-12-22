// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/layout_manager.h"

#include "base/macros.h"
#include "components/mus/public/cpp/tests/test_window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mash {
namespace wm {

class TestLayoutManager : public LayoutManager {
 public:
  explicit TestLayoutManager(mus::Window* window)
      : LayoutManager(window), layout_called_(false) {}
  ~TestLayoutManager() override {}

  bool GetAndResetLayoutCalled() {
    bool was_layout_called = layout_called_;
    layout_called_ = false;
    return was_layout_called;
  }

 private:
  // LayoutManager:
  void LayoutWindow(mus::Window* window) override {
    layout_called_ = true;
  }

  bool layout_called_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Tests that owning window can be destroyed before the layout manager.
TEST(LayoutManagerTest, OwningWindowDestroyedFirst) {
  scoped_ptr<mus::TestWindow> parent(new mus::TestWindow(1));
  mus::TestWindow child(2);
  TestLayoutManager layout_manager(parent.get());
  parent->AddChild(&child);
  EXPECT_TRUE(layout_manager.GetAndResetLayoutCalled());
  parent.reset();
  child.SetBounds(gfx::Rect(100, 200));
}

// Tests that the layout manager can be destroyed before the owning window.
TEST(LayoutManagerTest, LayoutManagerDestroyedFirst) {
  mus::TestWindow parent(1);
  mus::TestWindow child(2);
  scoped_ptr<TestLayoutManager> layout_manager(new TestLayoutManager(&parent));
  parent.AddChild(&child);
  EXPECT_TRUE(layout_manager->GetAndResetLayoutCalled());

  parent.SetBounds(gfx::Rect(100, 200));
  EXPECT_TRUE(layout_manager->GetAndResetLayoutCalled());

  layout_manager.reset();
  parent.SetBounds(gfx::Rect(200, 100));
}

}  // namespace wm
}  // namespace mash
