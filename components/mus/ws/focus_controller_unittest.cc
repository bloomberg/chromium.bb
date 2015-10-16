// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/focus_controller.h"

#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/server_view.h"
#include "components/mus/ws/test_server_view_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {
namespace {

class TestFocusControllerDelegate : public FocusControllerDelegate {
 public:
  TestFocusControllerDelegate()
      : change_count_(0u),
        old_focused_view_(nullptr),
        new_focused_view_(nullptr) {}

  void ClearAll() {
    change_count_ = 0u;
    old_focused_view_ = nullptr;
    new_focused_view_ = nullptr;
  }
  size_t change_count() const { return change_count_; }
  ServerView* old_focused_view() { return old_focused_view_; }
  ServerView* new_focused_view() { return new_focused_view_; }

 private:
  // FocusControllerDelegate:
  void OnFocusChanged(ServerView* old_focused_view,
                      ServerView* new_focused_view) override {
    change_count_++;
    old_focused_view_ = old_focused_view;
    new_focused_view_ = new_focused_view;
  }

  size_t change_count_;
  ServerView* old_focused_view_;
  ServerView* new_focused_view_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusControllerDelegate);
};

}  // namespace

TEST(FocusControllerTest, Basic) {
  TestServerViewDelegate server_view_delegate;
  ServerView root(&server_view_delegate, ViewId());
  server_view_delegate.set_root_view(&root);
  root.SetVisible(true);
  ServerView child(&server_view_delegate, ViewId());
  child.SetVisible(true);
  root.Add(&child);
  ServerView child_child(&server_view_delegate, ViewId());
  child_child.SetVisible(true);
  child.Add(&child_child);

  TestFocusControllerDelegate focus_delegate;
  FocusController focus_controller(&focus_delegate);

  focus_controller.SetFocusedView(&child_child);
  EXPECT_EQ(0u, focus_delegate.change_count());

  // Remove the ancestor of the focused view, focus should go to the |root|.
  root.Remove(&child);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(&root, focus_delegate.new_focused_view());
  EXPECT_EQ(&child_child, focus_delegate.old_focused_view());
  focus_delegate.ClearAll();

  // Make the focused view invisible. Focus is lost in this case (as no one
  // to give focus to).
  root.SetVisible(false);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(nullptr, focus_delegate.new_focused_view());
  EXPECT_EQ(&root, focus_delegate.old_focused_view());
  focus_delegate.ClearAll();

  // Go back to initial state and focus |child_child|.
  root.SetVisible(true);
  root.Add(&child);
  focus_controller.SetFocusedView(&child_child);
  EXPECT_EQ(0u, focus_delegate.change_count());

  // Hide the focused view, focus should go to parent.
  child_child.SetVisible(false);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(&child, focus_delegate.new_focused_view());
  EXPECT_EQ(&child_child, focus_delegate.old_focused_view());
  focus_delegate.ClearAll();

  child_child.SetVisible(true);
  focus_controller.SetFocusedView(&child_child);
  EXPECT_EQ(0u, focus_delegate.change_count());

  // Hide the parent of the focused view.
  child.SetVisible(false);
  EXPECT_EQ(1u, focus_delegate.change_count());
  EXPECT_EQ(&root, focus_delegate.new_focused_view());
  EXPECT_EQ(&child_child, focus_delegate.old_focused_view());
  focus_delegate.ClearAll();
}

}  // namespace mus
