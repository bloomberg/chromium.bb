// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_view_drawn_tracker.h"

#include "components/mus/ws/server_view.h"
#include "components/mus/ws/server_view_drawn_tracker_observer.h"
#include "components/mus/ws/test_server_view_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {
namespace {

class TestServerViewDrawnTrackerObserver
    : public ServerViewDrawnTrackerObserver {
 public:
  TestServerViewDrawnTrackerObserver()
      : change_count_(0u),
        ancestor_(nullptr),
        view_(nullptr),
        is_drawn_(false) {}

  void clear_change_count() { change_count_ = 0u; }
  size_t change_count() const { return change_count_; }
  const ServerView* ancestor() const { return ancestor_; }
  const ServerView* view() const { return view_; }
  bool is_drawn() const { return is_drawn_; }

 private:
  // ServerViewDrawnTrackerObserver:
  void OnDrawnStateChanged(ServerView* ancestor,
                           ServerView* view,
                           bool is_drawn) override {
    change_count_++;
    ancestor_ = ancestor;
    view_ = view;
    is_drawn_ = is_drawn;
  }

  size_t change_count_;
  const ServerView* ancestor_;
  const ServerView* view_;
  bool is_drawn_;

  DISALLOW_COPY_AND_ASSIGN(TestServerViewDrawnTrackerObserver);
};

}  // namespace

TEST(ServerViewDrawnTrackerTest, ChangeBecauseOfDeletionAndVisibility) {
  TestServerViewDelegate server_view_delegate;
  scoped_ptr<ServerView> view(new ServerView(&server_view_delegate, ViewId()));
  server_view_delegate.set_root_view(view.get());
  TestServerViewDrawnTrackerObserver drawn_observer;
  ServerViewDrawnTracker tracker(view.get(), &drawn_observer);
  view->SetVisible(true);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(view.get(), drawn_observer.view());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  view->SetVisible(false);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(view.get(), drawn_observer.view());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  view->SetVisible(true);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(view.get(), drawn_observer.view());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  view.reset();
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(view.get(), drawn_observer.view());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
}

TEST(ServerViewDrawnTrackerTest, ChangeBecauseOfRemovingFromRoot) {
  TestServerViewDelegate server_view_delegate;
  ServerView root(&server_view_delegate, ViewId());
  server_view_delegate.set_root_view(&root);
  root.SetVisible(true);
  ServerView child(&server_view_delegate, ViewId());
  child.SetVisible(true);
  root.Add(&child);

  TestServerViewDrawnTrackerObserver drawn_observer;
  ServerViewDrawnTracker tracker(&child, &drawn_observer);
  root.Remove(&child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child, drawn_observer.view());
  EXPECT_EQ(&root, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  root.Add(&child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child, drawn_observer.view());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
}

TEST(ServerViewDrawnTrackerTest, ChangeBecauseOfRemovingAncestorFromRoot) {
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

  TestServerViewDrawnTrackerObserver drawn_observer;
  ServerViewDrawnTracker tracker(&child_child, &drawn_observer);
  root.Remove(&child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child_child, drawn_observer.view());
  EXPECT_EQ(&root, drawn_observer.ancestor());
  EXPECT_FALSE(drawn_observer.is_drawn());
  drawn_observer.clear_change_count();

  root.Add(&child_child);
  EXPECT_EQ(1u, drawn_observer.change_count());
  EXPECT_EQ(&child_child, drawn_observer.view());
  EXPECT_EQ(nullptr, drawn_observer.ancestor());
  EXPECT_TRUE(drawn_observer.is_drawn());
}

}  // namespace mus
