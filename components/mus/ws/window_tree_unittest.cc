// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "components/mus/common/types.h"
#include "components/mus/common/util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_factory.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {
namespace ws {
namespace test {
namespace {

std::string WindowIdToString(const WindowId& id) {
  return base::StringPrintf("%d,%d", id.connection_id, id.window_id);
}

ClientWindowId BuildClientWindowId(WindowTree* tree,
                                   ConnectionSpecificId window_id) {
  return ClientWindowId(WindowIdToTransportId(WindowId(tree->id(), window_id)));
}

ClientWindowId ClientWindowIdForWindow(WindowTree* tree,
                                       const ServerWindow* window) {
  ClientWindowId client_window_id;
  // If window isn't known we'll return 0, which should then error out.
  tree->IsWindowKnown(window, &client_window_id);
  return client_window_id;
}

class TestWindowManager : public mojom::WindowManager {
 public:
  TestWindowManager() : got_create_top_level_window_(false), change_id_(0u) {}
  ~TestWindowManager() override {}

  bool did_call_create_top_level_window(uint32_t* change_id) {
    if (!got_create_top_level_window_)
      return false;

    got_create_top_level_window_ = false;
    *change_id = change_id_;
    return true;
  }

 private:
  // WindowManager:
  void WmSetBounds(uint32_t change_id,
                   uint32_t window_id,
                   mojo::RectPtr bounds) override {}
  void WmSetProperty(uint32_t change_id,
                     uint32_t window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> value) override {}
  void WmCreateTopLevelWindow(
      uint32_t change_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override {
    got_create_top_level_window_ = true;
    change_id_ = change_id;
  }
  void OnAccelerator(uint32_t id, mojom::EventPtr event) override {}

  bool got_create_top_level_window_;
  uint32_t change_id_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManager);
};

// -----------------------------------------------------------------------------

class TestDisplayBinding : public DisplayBinding {
 public:
  TestDisplayBinding(Display* display, ConnectionManager* manager)
      : display_(display), connection_manager_(manager) {}
  ~TestDisplayBinding() override {}

 private:
  // DisplayBinding:
  WindowTree* CreateWindowTree(ServerWindow* root) override {
    return connection_manager_->EmbedAtWindow(
        root, mus::mojom::WindowTree::kAccessPolicyEmbedRoot,
        mus::mojom::WindowTreeClientPtr());
  }

  Display* display_;
  ConnectionManager* connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayBinding);
};

ui::PointerEvent CreatePointerDownEvent(int x, int y) {
  return ui::PointerEvent(ui::TouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(x, y),
                                         1, ui::EventTimeForNow()));
}

ui::PointerEvent CreatePointerUpEvent(int x, int y) {
  return ui::PointerEvent(ui::TouchEvent(
      ui::ET_TOUCH_RELEASED, gfx::Point(x, y), 1, ui::EventTimeForNow()));
}

ui::PointerEvent CreateMouseMoveEvent(int x, int y) {
  return ui::PointerEvent(
      ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(x, y), gfx::Point(x, y),
                     ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE));
}

ui::PointerEvent CreateMouseDownEvent(int x, int y) {
  return ui::PointerEvent(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(x, y), gfx::Point(x, y),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
}

ui::PointerEvent CreateMouseUpEvent(int x, int y) {
  return ui::PointerEvent(
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(x, y), gfx::Point(x, y),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
}

const ServerWindow* FirstRoot(WindowTree* tree) {
  return tree->roots().size() == 1u ? *(tree->roots().begin()) : nullptr;
}

ClientWindowId FirstRootId(WindowTree* tree) {
  return tree->roots().size() == 1u
             ? ClientWindowIdForWindow(tree, *(tree->roots().begin()))
             : ClientWindowId();
}

}  // namespace

// -----------------------------------------------------------------------------

class WindowTreeTest : public testing::Test {
 public:
  WindowTreeTest()
      : wm_client_(nullptr),
        cursor_id_(0),
        platform_display_factory_(&cursor_id_) {}
  ~WindowTreeTest() override {}

  // WindowTree for the window manager.
  WindowTree* wm_tree() { return connection_manager_->GetTreeWithId(1); }

  TestWindowTreeClient* last_window_tree_client() {
    return delegate_.last_client();
  }

  TestWindowTreeBinding* last_binding() { return delegate_.last_binding(); }

  ConnectionManager* connection_manager() { return connection_manager_.get(); }

  ServerWindow* GetWindowById(const WindowId& id) {
    return connection_manager_->GetWindow(id);
  }

  TestWindowTreeClient* wm_client() { return wm_client_; }
  mus::mojom::Cursor cursor_id() {
    return static_cast<mus::mojom::Cursor>(cursor_id_);
  }

  void DispatchEventWithoutAck(const ui::Event& event) {
    DisplayTestApi(display_).OnEvent(event);
  }

  void set_window_manager_internal(WindowTree* tree,
                                   mojom::WindowManager* wm_internal) {
    WindowTreeTestApi(tree).set_window_manager_internal(wm_internal);
  }

  void AckPreviousEvent() {
    DisplayTestApi test_api(display_);
    while (test_api.tree_awaiting_input_ack())
      test_api.tree_awaiting_input_ack()->OnWindowInputEventAck(0);
  }

  void DispatchEventAndAckImmediately(const ui::Event& event) {
    DispatchEventWithoutAck(event);
    AckPreviousEvent();
  }

  // Creates a new window from wm_tree() and embeds a new connection in
  // it.
  void SetupEventTargeting(TestWindowTreeClient** out_client,
                           WindowTree** window_tree,
                           ServerWindow** window);

 protected:
  // testing::Test:
  void SetUp() override {
    PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
    connection_manager_.reset(
        new ConnectionManager(&delegate_, scoped_refptr<SurfacesState>()));
    display_ = new Display(connection_manager_.get(), nullptr,
                           scoped_refptr<GpuState>(),
                           scoped_refptr<mus::SurfacesState>());
    display_binding_ =
        new TestDisplayBinding(display_, connection_manager_.get());
    display_->Init(make_scoped_ptr(display_binding_));
    wm_client_ = delegate_.last_client();
  }

 protected:
  // TestWindowTreeClient that is used for the WM connection.
  TestWindowTreeClient* wm_client_;
  int32_t cursor_id_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestConnectionManagerDelegate delegate_;
  // Owned by ConnectionManager.
  TestDisplayBinding* display_binding_;
  Display* display_ = nullptr;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTest);
};

// Creates a new window in wm_tree(), adds it to the root, embeds a
// new client in the window and creates a child of said window. |window| is
// set to the child of |window_tree| that is created.
void WindowTreeTest::SetupEventTargeting(TestWindowTreeClient** out_client,
                                         WindowTree** window_tree,
                                         ServerWindow** window) {
  const ClientWindowId embed_window_id = BuildClientWindowId(wm_tree(), 1);
  EXPECT_TRUE(
      wm_tree()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_tree()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(wm_tree()->AddWindow(FirstRootId(wm_tree()), embed_window_id));
  display_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = GetProxy(&client);
  wm_client()->Bind(std::move(client_request));
  ConnectionSpecificId connection_id = 0;
  wm_tree()->Embed(embed_window_id, std::move(client),
                   mojom::WindowTree::kAccessPolicyDefault, &connection_id);
  ServerWindow* embed_window = wm_tree()->GetWindowByClientId(embed_window_id);
  WindowTree* tree1 = connection_manager()->GetTreeWithRoot(embed_window);
  ASSERT_TRUE(tree1 != nullptr);
  ASSERT_NE(tree1, wm_tree());

  embed_window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ClientWindowId child1_id(BuildClientWindowId(tree1, 1));
  EXPECT_TRUE(tree1->NewWindow(child1_id, ServerWindow::Properties()));
  ServerWindow* child1 = tree1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  EXPECT_TRUE(tree1->AddWindow(ClientWindowIdForWindow(tree1, embed_window),
                               child1_id));
  tree1->GetDisplay(embed_window)->AddActivationParent(embed_window);

  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child1);

  TestWindowTreeClient* embed_connection = last_window_tree_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  *out_client = embed_connection;
  *window_tree = tree1;
  *window = child1;
}

// Verifies focus correctly changes on pointer events.
TEST_F(WindowTreeTest, FocusOnPointer) {
  const ClientWindowId embed_window_id = BuildClientWindowId(wm_tree(), 1);
  EXPECT_TRUE(
      wm_tree()->NewWindow(embed_window_id, ServerWindow::Properties()));
  ServerWindow* embed_window = wm_tree()->GetWindowByClientId(embed_window_id);
  ASSERT_TRUE(embed_window);
  EXPECT_TRUE(wm_tree()->SetWindowVisibility(embed_window_id, true));
  ASSERT_TRUE(FirstRoot(wm_tree()));
  const ClientWindowId wm_root_id = FirstRootId(wm_tree());
  EXPECT_TRUE(wm_tree()->AddWindow(wm_root_id, embed_window_id));
  display_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = GetProxy(&client);
  wm_client()->Bind(std::move(client_request));
  ConnectionSpecificId connection_id = 0;
  wm_tree()->Embed(embed_window_id, std::move(client),
                   mojom::WindowTree::kAccessPolicyDefault, &connection_id);
  WindowTree* tree1 = connection_manager()->GetTreeWithRoot(embed_window);
  ASSERT_TRUE(tree1 != nullptr);
  ASSERT_NE(tree1, wm_tree());

  embed_window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ClientWindowId child1_id(BuildClientWindowId(tree1, 1));
  EXPECT_TRUE(tree1->NewWindow(child1_id, ServerWindow::Properties()));
  EXPECT_TRUE(tree1->AddWindow(ClientWindowIdForWindow(tree1, embed_window),
                               child1_id));
  ServerWindow* child1 = tree1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child1);

  TestWindowTreeClient* tree1_client = last_window_tree_client();
  tree1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  // Focus should not go to |child1| yet, since the parent still doesn't allow
  // active children.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  Display* display1 = tree1->GetDisplay(embed_window);
  EXPECT_EQ(nullptr, display1->GetFocusedWindow());
  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  tree1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  display1->AddActivationParent(embed_window);

  // Focus should go to child1. This result in notifying both the window
  // manager and client connection being notified.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(child1, display1->GetFocusedWindow());
  ASSERT_GE(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_GE(tree1_client->tracker()->changes()->size(), 1u);
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*tree1_client->tracker()->changes())[0]);

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  tree1_client->tracker()->changes()->clear();

  // Press outside of the embedded window. Note that root cannot be focused
  // (because it cannot be activated). So the focus would not move in this case.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(child1, display_->GetFocusedWindow());

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  tree1_client->tracker()->changes()->clear();

  // Press in the same location. Should not get a focus change event (only input
  // event).
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(child1, display_->GetFocusedWindow());
  ASSERT_EQ(wm_client()->tracker()->changes()->size(), 1u)
      << SingleChangeToDescription(*wm_client()->tracker()->changes());
  EXPECT_EQ("InputEvent window=0,3 event_action=4",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  EXPECT_TRUE(tree1_client->tracker()->changes()->empty());
}

TEST_F(WindowTreeTest, BasicInputEventTarget) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));

  // Send an event to |v1|. |embed_connection| should get the event, not
  // |wm_client|, since |v1| lives inside an embedded window.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_EQ(2u, embed_connection->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*embed_connection->tracker()->changes())[0]);
  EXPECT_EQ("InputEvent window=2,1 event_action=4",
            ChangesToDescription1(*embed_connection->tracker()->changes())[1]);
}

TEST_F(WindowTreeTest, CursorChangesWhenMouseOverWindowAndWindowSetsCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));

  // Like in BasicInputEventTarget, we send a pointer down event to be
  // dispatched. This is only to place the mouse cursor over that window though.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));

  window->SetPredefinedCursor(mojom::Cursor::IBEAM);

  // Because the cursor is over the window when SetCursor was called, we should
  // have immediately changed the cursor.
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, CursorChangesWhenEnteringWindowWithDifferentCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));

  // Let's create a pointer event outside the window and then move the pointer
  // inside.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, TouchesDontChangeCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));

  // Let's create a pointer event outside the window and then move the pointer
  // inside.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // With a touch event, we shouldn't update the cursor.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, DragOutsideWindow) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));

  // Start with the cursor outside the window. Setting the cursor shouldn't
  // change the cursor.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // Move the pointer to the inside of the window
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Start the drag.
  DispatchEventAndAckImmediately(CreateMouseDownEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Move the cursor (mouse is still down) outside the window.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Release the cursor. We should now adapt the cursor of the window
  // underneath the pointer.
  DispatchEventAndAckImmediately(CreateMouseUpEvent(5, 5));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, ChangingWindowBoundsChangesCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));

  // Put the cursor just outside the bounds of the window.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(41, 41));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // Expand the bounds of the window so they now include where the cursor now
  // is.
  window->SetBounds(gfx::Rect(20, 20, 25, 25));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Contract the bounds again.
  window->SetBounds(gfx::Rect(20, 20, 20, 20));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, WindowReorderingChangesCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window1 = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window1));

  // Create a second window right over the first.
  const ClientWindowId embed_window_id(FirstRootId(tree));
  const ClientWindowId child2_id(BuildClientWindowId(tree, 2));
  EXPECT_TRUE(tree->NewWindow(child2_id, ServerWindow::Properties()));
  ServerWindow* child2 = tree->GetWindowByClientId(child2_id);
  ASSERT_TRUE(child2);
  EXPECT_TRUE(tree->AddWindow(embed_window_id, child2_id));
  child2->SetVisible(true);
  child2->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child2);

  // Give each window a different cursor.
  window1->SetPredefinedCursor(mojom::Cursor::IBEAM);
  child2->SetPredefinedCursor(mojom::Cursor::HAND);

  // We expect window2 to be over window1 now.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(22, 22));
  EXPECT_EQ(mojom::Cursor::HAND, cursor_id());

  // But when we put window2 at the bottom, we should adapt window1's cursor.
  child2->parent()->StackChildAtBottom(child2);
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, EventAck) {
  const ClientWindowId embed_window_id = BuildClientWindowId(wm_tree(), 1);
  EXPECT_TRUE(
      wm_tree()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_tree()->SetWindowVisibility(embed_window_id, true));
  ASSERT_TRUE(FirstRoot(wm_tree()));
  EXPECT_TRUE(wm_tree()->AddWindow(FirstRootId(wm_tree()), embed_window_id));
  display_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));

  wm_client()->tracker()->changes()->clear();
  DispatchEventWithoutAck(CreateMouseMoveEvent(21, 22));
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("InputEvent window=0,3 event_action=5",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  wm_client()->tracker()->changes()->clear();

  // Send another event. This event shouldn't reach the client.
  DispatchEventWithoutAck(CreateMouseMoveEvent(21, 22));
  ASSERT_EQ(0u, wm_client()->tracker()->changes()->size());

  // Ack the first event. That should trigger the dispatch of the second event.
  AckPreviousEvent();
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("InputEvent window=0,3 event_action=5",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
}

// Establish connection, call NewTopLevelWindow(), make sure get id, and make
// sure client paused.
TEST_F(WindowTreeTest, NewTopLevelWindow) {
  TestWindowManager wm_internal;
  set_window_manager_internal(wm_tree(), &wm_internal);
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));
  embed_connection->tracker()->changes()->clear();
  embed_connection->set_record_on_change_completed(true);

  // Create a new top level window.
  mojo::Map<mojo::String, mojo::Array<uint8_t>> properties;
  const uint32_t initial_change_id = 17;
  // Explicitly use an id that does not contain the connection id.
  const ClientWindowId embed_window_id2_in_child(45 << 16 | 27);
  static_cast<mojom::WindowTree*>(tree)->NewTopLevelWindow(
      initial_change_id, embed_window_id2_in_child.id, std::move(properties));

  // The binding should be paused until the wm acks the change.
  uint32_t wm_change_id = 0u;
  ASSERT_TRUE(wm_internal.did_call_create_top_level_window(&wm_change_id));
  EXPECT_TRUE(last_binding()->is_paused());

  // Create the window for |embed_window_id2_in_child|.
  const ClientWindowId embed_window_id2 = BuildClientWindowId(wm_tree(), 2);
  EXPECT_TRUE(
      wm_tree()->NewWindow(embed_window_id2, ServerWindow::Properties()));
  EXPECT_TRUE(wm_tree()->AddWindow(FirstRootId(wm_tree()), embed_window_id2));

  // Ack the change, which should resume the binding.
  static_cast<mojom::WindowManagerClient*>(wm_tree())
      ->OnWmCreatedTopLevelWindow(wm_change_id, embed_window_id2.id);
  EXPECT_FALSE(last_binding()->is_paused());
  EXPECT_EQ("TopLevelCreated id=17 window_id=" +
                WindowIdToString(
                    WindowIdFromTransportId(embed_window_id2_in_child.id)),
            SingleChangeToDescription(*embed_connection->tracker()->changes()));
  embed_connection->tracker()->changes()->clear();

  // Change the visibility of the window from the owner and make sure the
  // client sees the right id.
  ServerWindow* embed_window = wm_tree()->GetWindowByClientId(embed_window_id2);
  ASSERT_TRUE(embed_window);
  EXPECT_FALSE(embed_window->visible());
  ASSERT_TRUE(wm_tree()->SetWindowVisibility(
      ClientWindowIdForWindow(wm_tree(), embed_window), true));
  EXPECT_TRUE(embed_window->visible());
  EXPECT_EQ("VisibilityChanged window=" +
                WindowIdToString(
                    WindowIdFromTransportId(embed_window_id2_in_child.id)) +
                " visible=true",
            SingleChangeToDescription(*embed_connection->tracker()->changes()));

  // Set the visibility from the child using the client assigned id.
  ASSERT_TRUE(tree->SetWindowVisibility(embed_window_id2_in_child, false));
  EXPECT_FALSE(embed_window->visible());
}

// Tests that setting capture only works while an input event is being
// processed, and the only the capture window can release capture.
TEST_F(WindowTreeTest, ExplicitSetCapture) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* tree = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &tree, &window));
  const ServerWindow* root_window = *tree->roots().begin();
  tree->AddWindow(FirstRootId(tree), ClientWindowIdForWindow(tree, window));
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(tree->GetDisplay(window));

  // Setting capture should fail when there are no active events
  mojom::WindowTree* mojom_window_tree = static_cast<mojom::WindowTree*>(tree);
  uint32_t change_id = 42;
  mojom_window_tree->SetCapture(change_id, WindowIdToTransportId(window->id()));
  Display* display = tree->GetDisplay(window);
  EXPECT_NE(window, display->GetCaptureWindow());

  // Setting capture after the event is acknowledged should fail
  DispatchEventAndAckImmediately(CreatePointerDownEvent(10, 10));
  mojom_window_tree->SetCapture(++change_id,
                                WindowIdToTransportId(window->id()));
  EXPECT_NE(window, display->GetCaptureWindow());

  // Settings while the event is being process should pass
  DispatchEventWithoutAck(CreatePointerDownEvent(10, 10));
  mojom_window_tree->SetCapture(++change_id,
                                WindowIdToTransportId(window->id()));
  EXPECT_EQ(window, display->GetCaptureWindow());
  AckPreviousEvent();

  // Only the capture window should be able to release capture
  mojom_window_tree->ReleaseCapture(++change_id,
                                    WindowIdToTransportId(root_window->id()));
  EXPECT_EQ(window, display->GetCaptureWindow());
  mojom_window_tree->ReleaseCapture(++change_id,
                                    WindowIdToTransportId(window->id()));
  EXPECT_EQ(nullptr, display->GetCaptureWindow());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
