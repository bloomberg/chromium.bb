// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/transient_window_manager.h"
#include "components/mus/ws/transient_window_observer.h"
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

void AddTransientChild(ServerWindow* parent, ServerWindow* child) {
  TransientWindowManager::GetOrCreate(parent)->AddTransientChild(child);
}

}  // namespace

class TransientWindowManagerTest : public testing::Test {
 public:
  TransientWindowManagerTest() {}
  ~TransientWindowManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TransientWindowManagerTest);
};

TEST(TransientWindowManagerTest, TransientChildren) {
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
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).
  w1.reset();
  w2 = nullptr;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
}

}  // namespace ws

}  // namespace mus
