// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window_drawn_tracker.h"

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_drawn_tracker_observer.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {
namespace {

class TestServerWindowDrawnTrackerObserver
    : public ServerWindowDrawnTrackerObserver {
 public:
  TestServerWindowDrawnTrackerObserver()
      : change_count_(0u),
        ancestor_(nullptr),
        window_(nullptr),
        is_drawn_(false) {}

  void clear_change_count() { change_count_ = 0u; }
  size_t change_count() const { return change_count_; }
  const ServerWindow* ancestor() const { return ancestor_; }
  const ServerWindow* window() const { return window_; }
  bool is_drawn() const { return is_drawn_; }

 private:
  // ServerWindowDrawnTrackerObserver:
  void OnDrawnStateChanged(ServerWindow* ancestor,
                           ServerWindow* window,
                           bool is_drawn) override {
    change_count_++;
    ancestor_ = ancestor;
    window_ = window;
    is_drawn_ = is_drawn;
  }

  size_t change_count_;
  const ServerWindow* ancestor_;
  const ServerWindow* window_;
  bool is_drawn_;

  DISALLOW_COPY_AND_ASSIGN(TestServerWindowDrawnTrackerObserver);
};

}  // namespace

TEST(ServerWindowDrawnTrackerTest, ChangeBecauseOfDeletionAndVisibility) {
  TestServerWindowDelegate server_window_delegate;
  scoped_ptr<ServerWindow> window(
      new ServerWindow(&server_window_delegate, WindowId()));
  server_window_delegate.set_root_window(window.get());
  TestServerWindowDrawnTrackerObserver drawn_observer;
  ServerWindowDrawnTracker tracker(window.get(), &drawn_observer);
  window->SetVisible(true);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(window.get(), drawn_observer.window());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  window->SetVisible(false);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(window.get(), drawn_observer.window());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  window->SetVisible(true);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(window.get(), drawn_observer.window());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  window.reset();
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(window.get(), drawn_observer.window());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
}

TEST(ServerWindowDrawnTrackerTest, ChangeBecauseOfRemovingFromRoot) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow root(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&root);
  root.SetVisible(true);
  ServerWindow child(&server_window_delegate, WindowId());
  child.SetVisible(true);
  root.Add(&child);

  TestServerWindowDrawnTrackerObserver drawn_observer;
  ServerWindowDrawnTracker tracker(&child, &drawn_observer);
  root.Remove(&child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child, drawn_observer.window());
  EXPECT_EQ(&root, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  root.Add(&child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child, drawn_observer.window());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
}

TEST(ServerWindowDrawnTrackerTest, ChangeBecauseOfRemovingAncestorFromRoot) {
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

  TestServerWindowDrawnTrackerObserver drawn_observer;
  ServerWindowDrawnTracker tracker(&child_child, &drawn_observer);
  root.Remove(&child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child_child, drawn_observer.window());
  EXPECT_EQ(&root, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  root.Add(&child_child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child_child, drawn_observer.window());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
}

}  // namespace mus
