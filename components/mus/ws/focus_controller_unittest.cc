// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/focus_controller.h"

#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {

namespace ws {
namespace {

class TestFocusControllerDelegate : public FocusControllerDelegate {
 public:
  TestFocusControllerDelegate()
      : change_count_(0u),
        old_focused_window_(nullptr),
        new_focused_window_(nullptr) {}

  void ClearAll() {
    change_count_ = 0u;
    old_focused_window_ = nullptr;
    new_focused_window_ = nullptr;
  }
  size_t change_count() const { return change_count_; }
  ServerWindow* old_focused_window() { return old_focused_window_; }
  ServerWindow* new_focused_window() { return new_focused_window_; }

 private:
  // FocusControllerDelegate:
  void OnFocusChanged(ServerWindow* old_focused_window,
                      ServerWindow* new_focused_window) override {
    change_count_++;
    old_focused_window_ = old_focused_window;
    new_focused_window_ = new_focused_window;
  }

  size_t change_count_;
  ServerWindow* old_focused_window_;
  ServerWindow* new_focused_window_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusControllerDelegate);
};

}  // namespace

TEST(FocusControllerTest, Basic) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow root(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&root);
  root.SetVisible(true);
  ServerWindow child(&server_window_delegate, WindowId());
  child.SetVisible(true);
  root.Add(&child);
  ServerWindow child_child(&server_window_delegate, WindowId());
  child_child.SetVisible(true);
  child.Add(&child_child);

  TestFocusControllerDelegate focus_delegate;
  FocusController focus_controller(&focus_delegate);

  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_delegate.change_count());

  // Remove the ancestor of the focused window, focus should go to the |root|.
  root.Remove(&child);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(&root, focus_delegate.new_focused_window());
  EXPECT_EQ(&child_child, focus_delegate.old_focused_window());
  focus_delegate.ClearAll();

  // Make the focused window invisible. Focus is lost in this case (as no one
  // to give focus to).
  root.SetVisible(false);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(nullptr, focus_delegate.new_focused_window());
  EXPECT_EQ(&root, focus_delegate.old_focused_window());
  focus_delegate.ClearAll();

  // Go back to initial state and focus |child_child|.
  root.SetVisible(true);
  root.Add(&child);
  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_delegate.change_count());

  // Hide the focused window, focus should go to parent.
  child_child.SetVisible(false);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(&child, focus_delegate.new_focused_window());
  EXPECT_EQ(&child_child, focus_delegate.old_focused_window());
  focus_delegate.ClearAll();

  child_child.SetVisible(true);
  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_delegate.change_count());

  // Hide the parent of the focused window.
  child.SetVisible(false);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(&root, focus_delegate.new_focused_window());
  EXPECT_EQ(&child_child, focus_delegate.old_focused_window());
  focus_delegate.ClearAll();
}

}  // namespace ws

}  // namespace mus
