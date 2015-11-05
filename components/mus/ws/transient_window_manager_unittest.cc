// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/transient_window_manager.h"
#include "components/mus/ws/transient_window_observer.h"
#include "components/mus/ws/transient_window_stacking_client.h"
#include "components/mus/ws/window_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {
namespace ws {

namespace {

class TestTransientWindowObserver : public TransientWindowObserver {
 public:
  TestTransientWindowObserver() : add_count_(0), remove_count_(0) {}

  ~TestTransientWindowObserver() override {}

  int add_count() const { return add_count_; }
  int remove_count() const { return remove_count_; }

  // TransientWindowObserver overrides:
  void OnTransientChildAdded(ServerWindow* window,
                             ServerWindow* transient) override {
    add_count_++;
  }
  void OnTransientChildRemoved(ServerWindow* window,
                               ServerWindow* transient) override {
    remove_count_++;
  }

 private:
  int add_count_;
  int remove_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTransientWindowObserver);
};

ServerWindow* CreateTestWindow(TestServerWindowDelegate* delegate,
                               const WindowId& window_id,
                               ServerWindow* parent) {
  ServerWindow* window = new ServerWindow(delegate, window_id);
  window->SetVisible(true);
  if (parent)
    parent->Add(window);
  else
    delegate->set_root_window(window);
  return window;
}

std::string ChildWindowIDsAsString(ServerWindow* parent) {
  std::string result;
  for (auto i = parent->children().begin(); i != parent->children().end();
       ++i) {
    if (!result.empty())
      result += " ";
    result += base::IntToString(WindowIdToTransportId((*i)->id()));
  }
  return result;
}

}  // namespace

class TransientWindowManagerTest : public testing::Test {
 public:
  TransientWindowManagerTest() {}
  ~TransientWindowManagerTest() override {}

  void SetUp() override {
    client_.reset(new TransientWindowStackingClient);
    SetWindowStackingClient(client_.get());
  }

  void TearDown() override { SetWindowStackingClient(nullptr); }

 private:
  scoped_ptr<TransientWindowStackingClient> client_;
  DISALLOW_COPY_AND_ASSIGN(TransientWindowManagerTest);
};

TEST_F(TransientWindowManagerTest, TransientChildren) {
  TestServerWindowDelegate server_window_delegate;

  scoped_ptr<ServerWindow> parent(
      CreateTestWindow(&server_window_delegate, WindowId(), nullptr));
  scoped_ptr<ServerWindow> w1(
      CreateTestWindow(&server_window_delegate, WindowId(1, 1), parent.get()));
  scoped_ptr<ServerWindow> w3(
      CreateTestWindow(&server_window_delegate, WindowId(1, 2), parent.get()));

  ServerWindow* w2 =
      CreateTestWindow(&server_window_delegate, WindowId(1, 3), parent.get());

  // w2 is now owned by w1.
  AddTransientChild(w1.get(), w2);
  // Stack w1 at the top (end), this should force w2 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).
  w1.reset();
  w2 = nullptr;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
}

// Tests that transient children are stacked as a unit when using stack above.
TEST_F(TransientWindowManagerTest, TransientChildrenGroupAbove) {
  TestServerWindowDelegate server_window_delegate;

  scoped_ptr<ServerWindow> parent(
      CreateTestWindow(&server_window_delegate, WindowId(), nullptr));
  scoped_ptr<ServerWindow> w1(
      CreateTestWindow(&server_window_delegate, WindowId(0, 1), parent.get()));

  ServerWindow* w11 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 11), parent.get());
  scoped_ptr<ServerWindow> w2(
      CreateTestWindow(&server_window_delegate, WindowId(0, 2), parent.get()));

  ServerWindow* w21 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 21), parent.get());
  ServerWindow* w211 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 211), parent.get());
  ServerWindow* w212 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 212), parent.get());
  ServerWindow* w213 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 213), parent.get());
  ServerWindow* w22 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 22), parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w11 is now owned by w1.
  AddTransientChild(w1.get(), w11);
  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w21);
  // w22 is now owned by w2.
  AddTransientChild(w2.get(), w22);
  // w211 is now owned by w21.
  AddTransientChild(w21, w211);
  // w212 is now owned by w21.
  AddTransientChild(w21, w212);
  // w213 is now owned by w21.
  AddTransientChild(w21, w213);
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w1 at the top (end), this should force w11 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAtTop(w2.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w11, w2.get(), mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w21, w1.get(), mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w21, w22, mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w11, w21, mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w213, w21, mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent above its transient child.
  parent->Reorder(w21, w211, mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->Reorder(w2.get(), w1.get(), mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w11, w213, mojom::ORDER_DIRECTION_ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

TEST_F(TransientWindowManagerTest, TransienChildGroupBelow) {
  TestServerWindowDelegate server_window_delegate;

  scoped_ptr<ServerWindow> parent(
      CreateTestWindow(&server_window_delegate, WindowId(), nullptr));
  scoped_ptr<ServerWindow> w1(
      CreateTestWindow(&server_window_delegate, WindowId(0, 1), parent.get()));

  ServerWindow* w11 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 11), parent.get());
  scoped_ptr<ServerWindow> w2(
      CreateTestWindow(&server_window_delegate, WindowId(0, 2), parent.get()));

  ServerWindow* w21 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 21), parent.get());
  ServerWindow* w211 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 211), parent.get());
  ServerWindow* w212 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 212), parent.get());
  ServerWindow* w213 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 213), parent.get());
  ServerWindow* w22 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 22), parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w11 is now owned by w1.
  AddTransientChild(w1.get(), w11);
  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w21);
  // w22 is now owned by w2.
  AddTransientChild(w2.get(), w22);
  // w211 is now owned by w21.
  AddTransientChild(w21, w211);
  // w212 is now owned by w21.
  AddTransientChild(w21, w212);
  // w213 is now owned by w21.
  AddTransientChild(w21, w213);
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w2 at the bottom, this should force w11 to be last (on top of w1).
  // This also tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAtBottom(w2.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAtBottom(w1.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w21, w1.get(), mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w11, w2.get(), mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w22, w21, mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w21, w11, mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w213, w211, mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent below its transient child.
  parent->Reorder(w21, w211, mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w1.get(), w2.get(), mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  parent->Reorder(w213, w11, mojom::ORDER_DIRECTION_BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient windows are stacked properly when created.
TEST_F(TransientWindowManagerTest, StackUponCreation) {
  TestServerWindowDelegate delegate;
  scoped_ptr<ServerWindow> parent(
      CreateTestWindow(&delegate, WindowId(), nullptr));
  scoped_ptr<ServerWindow> window0(
      CreateTestWindow(&delegate, WindowId(0, 1), parent.get()));
  scoped_ptr<ServerWindow> window1(
      CreateTestWindow(&delegate, WindowId(0, 2), parent.get()));

  ServerWindow* window2 =
      CreateTestWindow(&delegate, WindowId(0, 3), parent.get());
  AddTransientChild(window0.get(), window2);
  EXPECT_EQ("1 3 2", ChildWindowIDsAsString(parent.get()));
}

// Tests that windows are restacked properly after a call to AddTransientChild()
// or RemoveTransientChild().
TEST_F(TransientWindowManagerTest, RestackUponAddOrRemoveTransientChild) {
  TestServerWindowDelegate delegate;
  scoped_ptr<ServerWindow> parent(
      CreateTestWindow(&delegate, WindowId(), nullptr));
  scoped_ptr<ServerWindow> windows[4];
  for (int i = 0; i < 4; i++)
    windows[i].reset(CreateTestWindow(&delegate, WindowId(0, i), parent.get()));

  EXPECT_EQ("0 1 2 3", ChildWindowIDsAsString(parent.get()));

  AddTransientChild(windows[0].get(), windows[2].get());
  EXPECT_EQ("0 2 1 3", ChildWindowIDsAsString(parent.get()));

  AddTransientChild(windows[0].get(), windows[3].get());
  EXPECT_EQ("0 2 3 1", ChildWindowIDsAsString(parent.get()));

  RemoveTransientChild(windows[0].get(), windows[2].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(parent.get()));

  RemoveTransientChild(windows[0].get(), windows[3].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(parent.get()));
}

// Verifies TransientWindowObserver is notified appropriately.
TEST_F(TransientWindowManagerTest, TransientWindowObserverNotified) {
  TestServerWindowDelegate delegate;
  scoped_ptr<ServerWindow> parent(
      CreateTestWindow(&delegate, WindowId(), nullptr));
  scoped_ptr<ServerWindow> w1(
      CreateTestWindow(&delegate, WindowId(0, 1), parent.get()));

  TestTransientWindowObserver test_observer;
  TransientWindowManager::GetOrCreate(parent.get())
      ->AddObserver(&test_observer);

  AddTransientChild(parent.get(), w1.get());
  EXPECT_EQ(1, test_observer.add_count());
  EXPECT_EQ(0, test_observer.remove_count());

  RemoveTransientChild(parent.get(), w1.get());
  EXPECT_EQ(1, test_observer.add_count());
  EXPECT_EQ(1, test_observer.remove_count());

  TransientWindowManager::Get(parent.get())->RemoveObserver(&test_observer);
}

}  // namespace ws
}  // namespace mus
