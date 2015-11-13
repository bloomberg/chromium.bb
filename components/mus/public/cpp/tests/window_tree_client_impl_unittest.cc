// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"

#include "base/logging.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/tests/test_window.h"
#include "components/mus/public/cpp/tests/test_window_tree.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

mojo::Array<uint8_t> Int32ToPropertyTransportValue(int32_t value) {
  const std::vector<uint8_t> bytes =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(value);
  mojo::Array<uint8_t> transport_value;
  transport_value.resize(bytes.size());
  memcpy(&transport_value.front(), &(bytes.front()), bytes.size());
  return transport_value.Pass();
}

class TestWindowTreeDelegate : public WindowTreeDelegate {
 public:
  TestWindowTreeDelegate() {}
  ~TestWindowTreeDelegate() override {}

  // WindowTreeDelegate:
  void OnEmbed(Window* root) override {}
  void OnConnectionLost(WindowTreeConnection* connection) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeDelegate);
};

class WindowTreeClientImplPrivate {
 public:
  WindowTreeClientImplPrivate(WindowTreeClientImpl* tree_client_impl)
      : tree_client_impl_(tree_client_impl) {}
  ~WindowTreeClientImplPrivate() {}

  void Init(mojom::WindowTree* window_tree, uint32 access_policy) {
    mojom::WindowDataPtr root_data(mojom::WindowData::New());
    root_data->parent_id = 0;
    root_data->window_id = 1;
    root_data->bounds = mojo::Rect::From(gfx::Rect());
    root_data->properties.mark_non_null();
    root_data->visible = true;
    root_data->drawn = true;
    root_data->viewport_metrics = mojom::ViewportMetrics::New();
    root_data->viewport_metrics->size_in_pixels =
        mojo::Size::From(gfx::Size(1000, 1000));
    root_data->viewport_metrics->device_pixel_ratio = 1;
    tree_client_impl_->OnEmbedImpl(window_tree, 1, root_data.Pass(), 0,
                                   access_policy);
  }

 private:
  WindowTreeClientImpl* tree_client_impl_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImplPrivate);
};

class WindowTreeSetup {
 public:
  WindowTreeSetup() : tree_client_(&window_tree_delegate_, nullptr, nullptr) {
    WindowTreeClientImplPrivate(&tree_client_)
        .Init(&window_tree_, mojom::WindowTree::ACCESS_POLICY_DEFAULT);
    window_tree_.GetAndClearChangeId(nullptr);
  }

  WindowTreeConnection* window_tree_connection() {
    return static_cast<WindowTreeConnection*>(&tree_client_);
  }

  mojom::WindowTreeClient* window_tree_client() {
    return static_cast<mojom::WindowTreeClient*>(&tree_client_);
  }

  TestWindowTree* window_tree() { return &window_tree_; }

 private:
  TestWindowTree window_tree_;
  TestWindowTreeDelegate window_tree_delegate_;
  WindowTreeClientImpl tree_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeSetup);
};

using WindowTreeClientImplTest = testing::Test;

// Verifies bounds are reverted if the server replied that the change failed.
TEST_F(WindowTreeClientImplTest, SetBoundsFailed) {
  WindowTreeSetup setup;
  Window* root = setup.window_tree_connection()->GetRoot();
  ASSERT_TRUE(root);
  const gfx::Rect original_bounds(root->bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root->bounds());
  root->SetBounds(new_bounds);
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_bounds, root->bounds());
}

// Simulates a bounds change, and while the bounds change is in flight the
// server replies with a new bounds and the original bounds change fails.
TEST_F(WindowTreeClientImplTest, SetBoundsFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.window_tree_connection()->GetRoot();
  ASSERT_TRUE(root);
  const gfx::Rect original_bounds(root->bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root->bounds());
  root->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, root->bounds());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a bounds change.
  const gfx::Rect server_changed_bounds(gfx::Rect(0, 0, 101, 102));
  setup.window_tree_client()->OnWindowBoundsChanged(
      root->id(), mojo::Rect::From(original_bounds),
      mojo::Rect::From(server_changed_bounds));

  // This shouldn't trigger the bounds changing yet.
  EXPECT_EQ(new_bounds, root->bounds());

  // Tell the client the change failed, which should trigger failing to the
  // most recent bounds from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(server_changed_bounds, root->bounds());

  // Simulate server changing back to original bounds. Should take immediately.
  setup.window_tree_client()->OnWindowBoundsChanged(
      root->id(), mojo::Rect::From(server_changed_bounds),
      mojo::Rect::From(original_bounds));
  EXPECT_EQ(original_bounds, root->bounds());
}

TEST_F(WindowTreeClientImplTest, TwoInFlightBoundsChangesBothCanceled) {
  WindowTreeSetup setup;
  Window* root = setup.window_tree_connection()->GetRoot();
  ASSERT_TRUE(root);
  const gfx::Rect original_bounds(root->bounds());
  const gfx::Rect bounds1(gfx::Rect(0, 0, 100, 100));
  const gfx::Rect bounds2(gfx::Rect(0, 0, 100, 102));
  root->SetBounds(bounds1);
  EXPECT_EQ(bounds1, root->bounds());
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));

  root->SetBounds(bounds2);
  EXPECT_EQ(bounds2, root->bounds());
  uint32_t change_id2;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id2));

  // Tell the client change 1 failed. As there is a still a change in flight
  // nothing should happen.
  setup.window_tree_client()->OnChangeCompleted(change_id1, false);
  EXPECT_EQ(bounds2, root->bounds());

  // And tell the client change 2 failed too. Should now fallback to original
  // bounds.
  setup.window_tree_client()->OnChangeCompleted(change_id2, false);
  EXPECT_EQ(original_bounds, root->bounds());
}

// Verifies properties are reverted if the server replied that the change
// failed.
TEST_F(WindowTreeClientImplTest, SetPropertyFailed) {
  WindowTreeSetup setup;
  Window* root = setup.window_tree_connection()->GetRoot();
  ASSERT_TRUE(root);
  ASSERT_FALSE(root->HasSharedProperty("foo"));
  const int32_t new_value = 11;
  root->SetSharedProperty("foo", new_value);
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(new_value, root->GetSharedProperty<int32_t>("foo"));
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_FALSE(root->HasSharedProperty("foo"));
}

// Simulates a property change, and while the property change is in flight the
// server replies with a new property and the original property change fails.
TEST_F(WindowTreeClientImplTest, SetPropertyFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.window_tree_connection()->GetRoot();
  ASSERT_TRUE(root);
  const int32_t value1 = 11;
  root->SetSharedProperty("foo", value1);
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(value1, root->GetSharedProperty<int32_t>("foo"));
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a different value.
  const int32_t server_value = 12;
  setup.window_tree_client()->OnWindowSharedPropertyChanged(
      root->id(), "foo", Int32ToPropertyTransportValue(server_value));

  // This shouldn't trigger the property changing yet.
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(value1, root->GetSharedProperty<int32_t>("foo"));

  // Tell the client the change failed, which should trigger failing to the
  // most recent value from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(server_value, root->GetSharedProperty<int32_t>("foo"));

  // Simulate server changing back to value1. Should take immediately.
  setup.window_tree_client()->OnWindowSharedPropertyChanged(
      root->id(), "foo", Int32ToPropertyTransportValue(value1));
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(value1, root->GetSharedProperty<int32_t>("foo"));
}

}  // namespace mus
