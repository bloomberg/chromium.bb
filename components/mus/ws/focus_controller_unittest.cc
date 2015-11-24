// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/focus_controller.h"

#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/focus_controller_observer.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {

namespace ws {
namespace {

class TestFocusControllerObserver : public FocusControllerObserver,
                                    public FocusControllerDelegate {
 public:
  TestFocusControllerObserver()
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
  bool CanHaveActiveChildren(ServerWindow* window) const override {
    return true;
  }
  // FocusControllerObserver:
  void OnActivationChanged(ServerWindow* old_active_window,
                           ServerWindow* new_active_window) override {}
  void OnFocusChanged(FocusControllerChangeSource source,
                      ServerWindow* old_focused_window,
                      ServerWindow* new_focused_window) override {
    if (source == FocusControllerChangeSource::EXPLICIT)
      return;

    change_count_++;
    old_focused_window_ = old_focused_window;
    new_focused_window_ = new_focused_window;
  }

  size_t change_count_;
  ServerWindow* old_focused_window_;
  ServerWindow* new_focused_window_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusControllerObserver);
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

  TestFocusControllerObserver focus_observer;
  FocusController focus_controller(&focus_observer, &root);
  focus_controller.AddObserver(&focus_observer);

  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_observer.change_count());

  // Remove the ancestor of the focused window, focus should go to the |root|.
  root.Remove(&child);
  EXPECT_EQ(1u, focus_observer.change_count());
  EXPECT_EQ(&root, focus_observer.new_focused_window());
  EXPECT_EQ(&child_child, focus_observer.old_focused_window());
  focus_observer.ClearAll();

  // Make the focused window invisible. Focus is lost in this case (as no one
  // to give focus to).
  root.SetVisible(false);
  EXPECT_EQ(1u, focus_observer.change_count());
  EXPECT_EQ(nullptr, focus_observer.new_focused_window());
  EXPECT_EQ(&root, focus_observer.old_focused_window());
  focus_observer.ClearAll();

  // Go back to initial state and focus |child_child|.
  root.SetVisible(true);
  root.Add(&child);
  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_observer.change_count());

  // Hide the focused window, focus should go to parent.
  child_child.SetVisible(false);
  EXPECT_EQ(1u, focus_observer.change_count());
  EXPECT_EQ(&child, focus_observer.new_focused_window());
  EXPECT_EQ(&child_child, focus_observer.old_focused_window());
  focus_observer.ClearAll();

  child_child.SetVisible(true);
  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_observer.change_count());

  // Hide the parent of the focused window.
  child.SetVisible(false);
  EXPECT_EQ(1u, focus_observer.change_count());
  EXPECT_EQ(&root, focus_observer.new_focused_window());
  EXPECT_EQ(&child_child, focus_observer.old_focused_window());
  focus_observer.ClearAll();
  focus_controller.RemoveObserver(&focus_observer);
}

}  // namespace ws

}  // namespace mus
