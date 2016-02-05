// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/tests/test_window.h"
#include "components/mus/public/cpp/tests/test_window_tree.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tracker.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

mojo::Array<uint8_t> Int32ToPropertyTransportValue(int32_t value) {
  const std::vector<uint8_t> bytes =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(value);
  mojo::Array<uint8_t> transport_value;
  transport_value.resize(bytes.size());
  memcpy(&transport_value.front(), &(bytes.front()), bytes.size());
  return transport_value;
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

  void Init(mojom::WindowTree* window_tree, uint32_t access_policy) {
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
    tree_client_impl_->OnEmbedImpl(window_tree, 1, std::move(root_data), 0,
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
        .Init(&window_tree_, mojom::WindowTree::kAccessPolicyDefault);
    window_tree_.GetAndClearChangeId(nullptr);
  }

  WindowTreeConnection* window_tree_connection() {
    return static_cast<WindowTreeConnection*>(&tree_client_);
  }

  mojom::WindowTreeClient* window_tree_client() {
    return static_cast<mojom::WindowTreeClient*>(&tree_client_);
  }

  TestWindowTree* window_tree() { return &window_tree_; }

  Window* GetFirstRoot() {
    return window_tree_connection()->GetRoots().empty()
               ? nullptr
               : *window_tree_connection()->GetRoots().begin();
  }

 private:
  TestWindowTree window_tree_;
  TestWindowTreeDelegate window_tree_delegate_;
  WindowTreeClientImpl tree_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeSetup);
};

class TestInputEventHandler : public InputEventHandler {
 public:
  TestInputEventHandler()
      : received_event_(false), should_manually_ack_(false) {}
  ~TestInputEventHandler() override {}

  void set_should_manually_ack() { should_manually_ack_ = true; }

  void AckEvent() {
    DCHECK(should_manually_ack_);
    DCHECK(!ack_callback_.is_null());
    ack_callback_.Run();
    ack_callback_ = base::Closure();
  }

  void Reset() {
    received_event_ = false;
    ack_callback_ = base::Closure();
  }
  bool received_event() const { return received_event_; }

 private:
  // InputEventHandler:
  void OnWindowInputEvent(Window* target,
                          mojom::EventPtr event,
                          scoped_ptr<base::Closure>* ack_callback) override {
    EXPECT_FALSE(received_event_)
        << "Observer was not reset after receiving event.";
    received_event_ = true;
    if (should_manually_ack_) {
      ack_callback_ = *ack_callback->get();
      ack_callback->reset();
    }
  }

  bool received_event_;
  bool should_manually_ack_;
  base::Closure ack_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestInputEventHandler);
};

using WindowTreeClientImplTest = testing::Test;

// Verifies bounds are reverted if the server replied that the change failed.
TEST_F(WindowTreeClientImplTest, SetBoundsFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
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
  Window* root = setup.GetFirstRoot();
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
  Window* root = setup.GetFirstRoot();
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
  Window* root = setup.GetFirstRoot();
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
  Window* root = setup.GetFirstRoot();
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

// Verifies visible is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientImplTest, SetVisibleFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const bool original_visible = root->visible();
  const bool new_visible = !original_visible;
  ASSERT_NE(new_visible, root->visible());
  root->SetVisible(new_visible);
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_visible, root->visible());
}

// Simulates a visible change, and while the visible change is in flight the
// server replies with a new visible and the original visible change fails.
TEST_F(WindowTreeClientImplTest, SetVisibleFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const bool original_visible = root->visible();
  const bool new_visible = !original_visible;
  ASSERT_NE(new_visible, root->visible());
  root->SetVisible(new_visible);
  EXPECT_EQ(new_visible, root->visible());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a visible change.
  const bool server_changed_visible = !new_visible;
  setup.window_tree_client()->OnWindowVisibilityChanged(root->id(),
                                                        server_changed_visible);

  // This shouldn't trigger visible changing yet.
  EXPECT_EQ(new_visible, root->visible());

  // Tell the client the change failed, which should trigger failing to the
  // most recent visible from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(server_changed_visible, root->visible());

  // Simulate server changing back to original visible. Should take immediately.
  setup.window_tree_client()->OnWindowVisibilityChanged(root->id(),
                                                        original_visible);
  EXPECT_EQ(original_visible, root->visible());
}

TEST_F(WindowTreeClientImplTest, InputEventBasic) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  TestInputEventHandler event_handler;
  root->set_input_event_handler(&event_handler);

  scoped_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  mojom::EventPtr mus_event = mojom::Event::From(*ui_event);
  setup.window_tree_client()->OnWindowInputEvent(1, root->id(),
                                                 std::move(mus_event));
  EXPECT_TRUE(event_handler.received_event());
  EXPECT_TRUE(setup.window_tree()->WasEventAcked(1));
  event_handler.Reset();

  event_handler.set_should_manually_ack();
  mus_event = mojom::Event::From(*ui_event);
  setup.window_tree_client()->OnWindowInputEvent(33, root->id(),
                                                 std::move(mus_event));
  EXPECT_TRUE(event_handler.received_event());
  EXPECT_FALSE(setup.window_tree()->WasEventAcked(33));

  event_handler.AckEvent();
  EXPECT_TRUE(setup.window_tree()->WasEventAcked(33));
}

// Verifies focus is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientImplTest, SetFocusFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  root->SetVisible(true);
  Window* child = setup.window_tree_connection()->NewWindow();
  child->SetVisible(true);
  root->AddChild(child);
  Window* original_focus = setup.window_tree_connection()->GetFocusedWindow();
  Window* new_focus = child;
  ASSERT_NE(new_focus, original_focus);
  new_focus->SetFocus();
  ASSERT_TRUE(new_focus->HasFocus());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_focus, setup.window_tree_connection()->GetFocusedWindow());
}

// Simulates a focus change, and while the focus change is in flight the server
// replies with a new focus and the original focus change fails.
TEST_F(WindowTreeClientImplTest, SetFocusFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  root->SetVisible(true);
  Window* child1 = setup.window_tree_connection()->NewWindow();
  child1->SetVisible(true);
  root->AddChild(child1);
  Window* child2 = setup.window_tree_connection()->NewWindow();
  child2->SetVisible(true);
  root->AddChild(child2);
  Window* original_focus = setup.window_tree_connection()->GetFocusedWindow();
  Window* new_focus = child1;
  ASSERT_NE(new_focus, original_focus);
  new_focus->SetFocus();
  ASSERT_TRUE(new_focus->HasFocus());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a focus change.
  setup.window_tree_client()->OnWindowFocused(child2->id());

  // This shouldn't trigger focus changing yet.
  EXPECT_TRUE(child1->HasFocus());

  // Tell the client the change failed, which should trigger failing to the
  // most recent focus from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_FALSE(child1->HasFocus());
  EXPECT_TRUE(child2->HasFocus());
  EXPECT_EQ(child2, setup.window_tree_connection()->GetFocusedWindow());

  // Simulate server changing focus to child1. Should take immediately.
  setup.window_tree_client()->OnWindowFocused(child1->id());
  EXPECT_TRUE(child1->HasFocus());
}

TEST_F(WindowTreeClientImplTest, FocusOnRemovedWindowWithInFlightFocusChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  root->SetVisible(true);
  Window* child1 = setup.window_tree_connection()->NewWindow();
  child1->SetVisible(true);
  root->AddChild(child1);
  Window* child2 = setup.window_tree_connection()->NewWindow();
  child2->SetVisible(true);
  root->AddChild(child2);

  child1->SetFocus();
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Destroy child1, which should set focus to null.
  child1->Destroy();
  EXPECT_EQ(nullptr, setup.window_tree_connection()->GetFocusedWindow());

  // Server changes focus to 2.
  setup.window_tree_client()->OnWindowFocused(child2->id());
  // Shouldn't take immediately.
  EXPECT_FALSE(child2->HasFocus());

  // Ack the change, focus should still be null.
  setup.window_tree_client()->OnChangeCompleted(change_id, true);
  EXPECT_EQ(nullptr, setup.window_tree_connection()->GetFocusedWindow());

  // Change to 2 again, this time it should take.
  setup.window_tree_client()->OnWindowFocused(child2->id());
  EXPECT_TRUE(child2->HasFocus());
}

class ToggleVisibilityFromDestroyedObserver : public WindowObserver {
 public:
  explicit ToggleVisibilityFromDestroyedObserver(Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ToggleVisibilityFromDestroyedObserver() { EXPECT_FALSE(window_); }

  // WindowObserver:
  void OnWindowDestroyed(Window* window) override {
    window_->SetVisible(!window->visible());
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ToggleVisibilityFromDestroyedObserver);
};

TEST_F(WindowTreeClientImplTest, ToggleVisibilityFromWindowDestroyed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  Window* child1 = setup.window_tree_connection()->NewWindow();
  root->AddChild(child1);
  ToggleVisibilityFromDestroyedObserver toggler(child1);
  // Destroying the window triggers
  // ToggleVisibilityFromDestroyedObserver::OnWindowDestroyed(), which toggles
  // the visibility of the window. Ack the change, which should not crash or
  // trigger DCHECKs.
  child1->Destroy();
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, true);
}

TEST_F(WindowTreeClientImplTest, NewTopLevelWindow) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.window_tree_connection()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);
  ASSERT_NE(root2, root1);
  EXPECT_NE(root2->id(), root1->id());
  EXPECT_EQ(2u, setup.window_tree_connection()->GetRoots().size());
  EXPECT_TRUE(setup.window_tree_connection()->GetRoots().count(root1) > 0u);
  EXPECT_TRUE(setup.window_tree_connection()->GetRoots().count(root2) > 0u);

  // Ack the request to the windowtree to create the new window.
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), root2->id());

  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = root2->id();
  data->viewport_metrics = mojom::ViewportMetrics::New();
  setup.window_tree_client()->OnTopLevelCreated(change_id, std::move(data));

  // Should not be able to add a top level as a child of another window.
  root1->AddChild(root2);
  ASSERT_EQ(nullptr, root2->parent());

  // Destroy the first root, shouldn't initiate tear down.
  root1->Destroy();
  root1 = nullptr;
  EXPECT_EQ(1u, setup.window_tree_connection()->GetRoots().size());
  EXPECT_TRUE(setup.window_tree_connection()->GetRoots().count(root2) > 0u);
}

TEST_F(WindowTreeClientImplTest, NewTopLevelWindowGetsPropertiesFromData) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.window_tree_connection()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);

  EXPECT_FALSE(root2->IsDrawn());
  EXPECT_FALSE(root2->visible());

  // Ack the request to the windowtree to create the new window.
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), root2->id());

  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = root2->id();
  data->viewport_metrics = mojom::ViewportMetrics::New();
  data->viewport_metrics->size_in_pixels = mojo::Size::From(gfx::Size(1, 2));
  data->bounds = mojo::Rect::From(gfx::Rect(1, 2, 3, 4));
  data->visible = true;
  data->drawn = true;
  setup.window_tree_client()->OnTopLevelCreated(change_id, std::move(data));

  // Make sure all the properties took.
  EXPECT_TRUE(root2->IsDrawn());
  EXPECT_TRUE(root2->visible());
  EXPECT_EQ(gfx::Size(1, 2),
            root2->viewport_metrics().size_in_pixels.To<gfx::Size>());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), root2->bounds());
}

TEST_F(WindowTreeClientImplTest, NewTopLevelWindowGetsAllChangesInFlight) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.window_tree_connection()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);

  EXPECT_FALSE(root2->IsDrawn());
  EXPECT_FALSE(root2->visible());

  // Get the id of the in flight change for creating the new window.
  uint32_t new_window_in_flight_change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(
      &new_window_in_flight_change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), root2->id());

  // Make visibility go from false->true->false. Don't ack immediately.
  root2->SetVisible(true);
  uint32_t vis_in_flight_change_id1;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&vis_in_flight_change_id1));
  EXPECT_NE(new_window_in_flight_change_id, vis_in_flight_change_id1);
  root2->SetVisible(false);
  uint32_t vis_in_flight_change_id2;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&vis_in_flight_change_id2));
  EXPECT_NE(vis_in_flight_change_id1, vis_in_flight_change_id2);

  // Change bounds to 5, 6, 7, 8.
  root2->SetBounds(gfx::Rect(5, 6, 7, 8));
  uint32_t bounds_in_flight_change_id;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&bounds_in_flight_change_id));
  EXPECT_NE(vis_in_flight_change_id2, bounds_in_flight_change_id);

  root2->SetSharedProperty<std::string>("xx", "client_xx");
  uint32_t property_in_flight_change_id;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&property_in_flight_change_id));
  EXPECT_NE(bounds_in_flight_change_id, property_in_flight_change_id);

  // Ack the new window top level window. Vis and bounds shouldn't change.
  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = root2->id();
  data->viewport_metrics = mojom::ViewportMetrics::New();
  data->viewport_metrics->size_in_pixels = mojo::Size::From(gfx::Size(1, 2));
  data->bounds = mojo::Rect::From(gfx::Rect(1, 2, 3, 4));
  data->visible = true;
  data->drawn = true;
  data->properties["xx"] = mojo::Array<uint8_t>::From(std::string("server_xx"));
  data->properties["yy"] = mojo::Array<uint8_t>::From(std::string("server_yy"));
  setup.window_tree_client()->OnTopLevelCreated(new_window_in_flight_change_id,
                                                std::move(data));

  // The only value that should take effect is the property for 'yy' as it was
  // not in flight.
  EXPECT_TRUE(WindowPrivate(root2).drawn());
  EXPECT_FALSE(root2->visible());
  EXPECT_EQ(gfx::Size(1, 2),
            root2->viewport_metrics().size_in_pixels.To<gfx::Size>());
  EXPECT_EQ(gfx::Rect(5, 6, 7, 8), root2->bounds());
  EXPECT_EQ(2u, root2->shared_properties().size());
  ASSERT_TRUE(root2->HasSharedProperty("yy"));
  EXPECT_EQ("server_yy", root2->GetSharedProperty<std::string>("yy"));
  ASSERT_TRUE(root2->HasSharedProperty("xx"));
  EXPECT_EQ("client_xx", root2->GetSharedProperty<std::string>("xx"));

  // Tell the client the changes failed. This should cause the values to change
  // to that of the server.
  setup.window_tree_client()->OnChangeCompleted(vis_in_flight_change_id1,
                                                false);
  EXPECT_FALSE(root2->visible());
  setup.window_tree_client()->OnChangeCompleted(vis_in_flight_change_id2,
                                                false);
  EXPECT_TRUE(root2->visible());
  setup.window_tree_client()->OnChangeCompleted(bounds_in_flight_change_id,
                                                false);
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), root2->bounds());
  setup.window_tree_client()->OnChangeCompleted(property_in_flight_change_id,
                                                false);
  EXPECT_EQ(2u, root2->shared_properties().size());
  ASSERT_TRUE(root2->HasSharedProperty("yy"));
  EXPECT_EQ("server_yy", root2->GetSharedProperty<std::string>("yy"));
  ASSERT_TRUE(root2->HasSharedProperty("xx"));
  EXPECT_EQ("server_xx", root2->GetSharedProperty<std::string>("xx"));
}

// Tests that if the client has multiple unowned windows, and one of them is a
// transient child to another, the  teardown can happen cleanly.
TEST_F(WindowTreeClientImplTest, MultipleUnOwnedWindowsDuringDestruction) {
  scoped_ptr<WindowTreeSetup> setup(new WindowTreeSetup());
  Window* root1 = setup->GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup->window_tree_connection()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);
  root1->AddTransientWindow(root2);

  WindowTracker tracker;
  tracker.Add(root1);
  tracker.Add(root2);
  setup.reset();
  EXPECT_TRUE(tracker.windows().empty());
}

TEST_F(WindowTreeClientImplTest, TopLevelWindowDestroyedBeforeCreateComplete) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.window_tree_connection()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);
  ASSERT_EQ(2u, setup.window_tree_connection()->GetRoots().size());

  // Get the id of the in flight change for creating the new window.
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), root2->id());

  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = root2->id();
  data->viewport_metrics = mojom::ViewportMetrics::New();

  // Destroy the window before the server has a chance to ack the window
  // creation.
  root2->Destroy();
  EXPECT_EQ(1u, setup.window_tree_connection()->GetRoots().size());

  setup.window_tree_client()->OnTopLevelCreated(change_id, std::move(data));
  EXPECT_EQ(1u, setup.window_tree_connection()->GetRoots().size());
}

}  // namespace mus
