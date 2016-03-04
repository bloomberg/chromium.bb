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
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/display_manager_factory.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_impl.h"
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

ClientWindowId BuildClientWindowId(WindowTreeImpl* tree,
                                   ConnectionSpecificId window_id) {
  return ClientWindowId(WindowIdToTransportId(WindowId(tree->id(), window_id)));
}

ClientWindowId ClientWindowIdForWindow(WindowTreeImpl* tree,
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

class TestWindowTreeHostConnection : public WindowTreeHostConnection {
 public:
  TestWindowTreeHostConnection(WindowTreeHostImpl* host_impl,
                               ConnectionManager* manager)
      : host_(host_impl), connection_manager_(manager) {}
  ~TestWindowTreeHostConnection() override {}

 private:
  // WindowTreeHostConnection:
  WindowTreeImpl* CreateWindowTree(ServerWindow* root) override {
    return connection_manager_->EmbedAtWindow(
        root, mus::mojom::WindowTree::kAccessPolicyEmbedRoot,
        mus::mojom::WindowTreeClientPtr());
  }

  WindowTreeHostImpl* host_;
  ConnectionManager* connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeHostConnection);
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

const ServerWindow* FirstRoot(WindowTreeImpl* connection) {
  return connection->roots().size() == 1u ? *(connection->roots().begin())
                                          : nullptr;
}

ClientWindowId FirstRootId(WindowTreeImpl* connection) {
  return connection->roots().size() == 1u
             ? ClientWindowIdForWindow(connection,
                                       *(connection->roots().begin()))
             : ClientWindowId();
}

}  // namespace

// -----------------------------------------------------------------------------

class WindowTreeTest : public testing::Test {
 public:
  WindowTreeTest()
      : wm_client_(nullptr),
        cursor_id_(0),
        display_manager_factory_(&cursor_id_) {}
  ~WindowTreeTest() override {}

  // WindowTreeImpl for the window manager.
  WindowTreeImpl* wm_connection() {
    return connection_manager_->GetConnection(1);
  }

  TestWindowTreeClient* last_window_tree_client() {
    return delegate_.last_client();
  }

  TestClientConnection* last_client_connection() {
    return delegate_.last_connection();
  }

  ConnectionManager* connection_manager() { return connection_manager_.get(); }

  ServerWindow* GetWindowById(const WindowId& id) {
    return connection_manager_->GetWindow(id);
  }

  TestWindowTreeClient* wm_client() { return wm_client_; }
  mus::mojom::Cursor cursor_id() {
    return static_cast<mus::mojom::Cursor>(cursor_id_);
  }

  TestWindowTreeHostConnection* host_connection() { return host_connection_; }

  void DispatchEventWithoutAck(const ui::Event& event) {
    WindowTreeHostTestApi(window_tree_host_).OnEvent(event);
  }

  void set_window_manager_internal(WindowTreeImpl* connection,
                                   mojom::WindowManager* wm_internal) {
    WindowTreeTestApi(connection).set_window_manager_internal(wm_internal);
  }

  void AckPreviousEvent() {
    WindowTreeHostTestApi test_api(window_tree_host_);
    while (test_api.tree_awaiting_input_ack())
      test_api.tree_awaiting_input_ack()->OnWindowInputEventAck(0);
  }

  void DispatchEventAndAckImmediately(const ui::Event& event) {
    DispatchEventWithoutAck(event);
    AckPreviousEvent();
  }

  // Creates a new window from wm_connection() and embeds a new connection in
  // it.
  void SetupEventTargeting(TestWindowTreeClient** out_client,
                           WindowTreeImpl** window_tree_connection,
                           ServerWindow** window);

 protected:
  // testing::Test:
  void SetUp() override {
    DisplayManager::set_factory_for_testing(&display_manager_factory_);
    connection_manager_.reset(
        new ConnectionManager(&delegate_, scoped_refptr<SurfacesState>()));
    window_tree_host_ = new WindowTreeHostImpl(
        connection_manager_.get(), nullptr, scoped_refptr<GpuState>(),
        scoped_refptr<mus::SurfacesState>());
    host_connection_ = new TestWindowTreeHostConnection(
        window_tree_host_, connection_manager_.get());
    window_tree_host_->Init(make_scoped_ptr(host_connection_));
    wm_client_ = delegate_.last_client();
  }

 protected:
  // TestWindowTreeClient that is used for the WM connection.
  TestWindowTreeClient* wm_client_;
  int32_t cursor_id_;
  TestDisplayManagerFactory display_manager_factory_;
  TestConnectionManagerDelegate delegate_;
  // Owned by ConnectionManager.
  TestWindowTreeHostConnection* host_connection_;
  WindowTreeHostImpl* window_tree_host_ = nullptr;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTest);
};

// Creates a new window in wm_connection(), adds it to the root, embeds a
// new client in the window and creates a child of said window. |window| is
// set to the child of |window_tree_connection| that is created.
void WindowTreeTest::SetupEventTargeting(
    TestWindowTreeClient** out_client,
    WindowTreeImpl** window_tree_connection,
    ServerWindow** window) {
  const ClientWindowId embed_window_id =
      BuildClientWindowId(wm_connection(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(wm_connection()->AddWindow(FirstRootId(wm_connection()),
                                         embed_window_id));
  window_tree_host_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = GetProxy(&client);
  wm_client()->Bind(std::move(client_request));
  ConnectionSpecificId connection_id = 0;
  wm_connection()->Embed(embed_window_id, std::move(client),
                         mojom::WindowTree::kAccessPolicyDefault,
                         &connection_id);
  ServerWindow* embed_window =
      wm_connection()->GetWindowByClientId(embed_window_id);
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  embed_window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ClientWindowId child1_id(BuildClientWindowId(connection1, 1));
  EXPECT_TRUE(connection1->NewWindow(child1_id, ServerWindow::Properties()));
  ServerWindow* child1 = connection1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  EXPECT_TRUE(connection1->AddWindow(
      ClientWindowIdForWindow(connection1, embed_window), child1_id));
  connection1->GetHost(embed_window)->AddActivationParent(embed_window);

  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child1);

  TestWindowTreeClient* embed_connection = last_window_tree_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  *out_client = embed_connection;
  *window_tree_connection = connection1;
  *window = child1;
}

// Verifies focus correctly changes on pointer events.
TEST_F(WindowTreeTest, FocusOnPointer) {
  const ClientWindowId embed_window_id =
      BuildClientWindowId(wm_connection(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  ServerWindow* embed_window =
      wm_connection()->GetWindowByClientId(embed_window_id);
  ASSERT_TRUE(embed_window);
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  ASSERT_TRUE(FirstRoot(wm_connection()));
  const ClientWindowId wm_root_id = FirstRootId(wm_connection());
  EXPECT_TRUE(wm_connection()->AddWindow(wm_root_id, embed_window_id));
  window_tree_host_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = GetProxy(&client);
  wm_client()->Bind(std::move(client_request));
  ConnectionSpecificId connection_id = 0;
  wm_connection()->Embed(embed_window_id, std::move(client),
                         mojom::WindowTree::kAccessPolicyDefault,
                         &connection_id);
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  embed_window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ClientWindowId child1_id(BuildClientWindowId(connection1, 1));
  EXPECT_TRUE(connection1->NewWindow(child1_id, ServerWindow::Properties()));
  EXPECT_TRUE(connection1->AddWindow(
      ClientWindowIdForWindow(connection1, embed_window), child1_id));
  ServerWindow* child1 = connection1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child1);

  TestWindowTreeClient* connection1_client = last_window_tree_client();
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  // Focus should not go to |child1| yet, since the parent still doesn't allow
  // active children.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  WindowTreeHostImpl* host1 = connection1->GetHost(embed_window);
  EXPECT_EQ(nullptr, host1->GetFocusedWindow());
  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  host1->AddActivationParent(embed_window);

  // Focus should go to child1. This result in notifying both the window
  // manager and client connection being notified.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(child1, host1->GetFocusedWindow());
  ASSERT_GE(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_GE(connection1_client->tracker()->changes()->size(), 1u);
  EXPECT_EQ(
      "Focused id=2,1",
      ChangesToDescription1(*connection1_client->tracker()->changes())[0]);

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press outside of the embedded window. Note that root cannot be focused
  // (because it cannot be activated). So the focus would not move in this case.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(child1, window_tree_host_->GetFocusedWindow());

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press in the same location. Should not get a focus change event (only input
  // event).
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(child1, window_tree_host_->GetFocusedWindow());
  ASSERT_EQ(wm_client()->tracker()->changes()->size(), 1u)
      << SingleChangeToDescription(*wm_client()->tracker()->changes());
  EXPECT_EQ("InputEvent window=0,3 event_action=4",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
}

TEST_F(WindowTreeTest, BasicInputEventTarget) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

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
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

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
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

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
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

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
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

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
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

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
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window1 = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(
      &embed_connection, &window_tree_connection, &window1));

  // Create a second window right over the first.
  const ClientWindowId embed_window_id(FirstRootId(window_tree_connection));
  const ClientWindowId child2_id(
      BuildClientWindowId(window_tree_connection, 2));
  EXPECT_TRUE(
      window_tree_connection->NewWindow(child2_id, ServerWindow::Properties()));
  ServerWindow* child2 = window_tree_connection->GetWindowByClientId(child2_id);
  ASSERT_TRUE(child2);
  EXPECT_TRUE(window_tree_connection->AddWindow(embed_window_id, child2_id));
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
  const ClientWindowId embed_window_id =
      BuildClientWindowId(wm_connection(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  ASSERT_TRUE(FirstRoot(wm_connection()));
  EXPECT_TRUE(wm_connection()->AddWindow(FirstRootId(wm_connection()),
                                         embed_window_id));
  window_tree_host_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));

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
  set_window_manager_internal(wm_connection(), &wm_internal);
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));
  embed_connection->tracker()->changes()->clear();
  embed_connection->set_record_on_change_completed(true);

  // Create a new top level window.
  mojo::Map<mojo::String, mojo::Array<uint8_t>> properties;
  const uint32_t initial_change_id = 17;
  // Explicitly use an id that does not contain the connection id.
  const ClientWindowId embed_window_id2_in_child(45 << 16 | 27);
  static_cast<mojom::WindowTree*>(window_tree_connection)
      ->NewTopLevelWindow(initial_change_id, embed_window_id2_in_child.id,
                          std::move(properties));

  // The binding should be paused until the wm acks the change.
  uint32_t wm_change_id = 0u;
  ASSERT_TRUE(wm_internal.did_call_create_top_level_window(&wm_change_id));
  EXPECT_TRUE(last_client_connection()->is_paused());

  // Create the window for |embed_window_id2_in_child|.
  const ClientWindowId embed_window_id2 =
      BuildClientWindowId(wm_connection(), 2);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id2, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->AddWindow(FirstRootId(wm_connection()),
                                         embed_window_id2));

  // Ack the change, which should resume the binding.
  static_cast<mojom::WindowManagerClient*>(wm_connection())
      ->OnWmCreatedTopLevelWindow(wm_change_id, embed_window_id2.id);
  EXPECT_FALSE(last_client_connection()->is_paused());
  EXPECT_EQ("TopLevelCreated id=17 window_id=" +
                WindowIdToString(
                    WindowIdFromTransportId(embed_window_id2_in_child.id)),
            SingleChangeToDescription(*embed_connection->tracker()->changes()));
  embed_connection->tracker()->changes()->clear();

  // Change the visibility of the window from the owner and make sure the
  // client sees the right id.
  ServerWindow* embed_window =
      wm_connection()->GetWindowByClientId(embed_window_id2);
  ASSERT_TRUE(embed_window);
  EXPECT_FALSE(embed_window->visible());
  ASSERT_TRUE(wm_connection()->SetWindowVisibility(
      ClientWindowIdForWindow(wm_connection(), embed_window), true));
  EXPECT_TRUE(embed_window->visible());
  EXPECT_EQ("VisibilityChanged window=" +
                WindowIdToString(
                    WindowIdFromTransportId(embed_window_id2_in_child.id)) +
                " visible=true",
            SingleChangeToDescription(*embed_connection->tracker()->changes()));

  // Set the visibility from the child using the client assigned id.
  ASSERT_TRUE(window_tree_connection->SetWindowVisibility(
      embed_window_id2_in_child, false));
  EXPECT_FALSE(embed_window->visible());
}

// Tests that setting capture only works while an input event is being
// processed, and the only the capture window can release capture.
TEST_F(WindowTreeTest, ExplicitSetCapture) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));
  const ServerWindow* root_window = *window_tree_connection->roots().begin();
  window_tree_connection->AddWindow(
      FirstRootId(window_tree_connection),
      ClientWindowIdForWindow(window_tree_connection, window));
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(window_tree_connection->GetHost(window));

  // Setting capture should fail when there are no active events
  mojom::WindowTree* mojom_window_tree =
      static_cast<mojom::WindowTree*>(window_tree_connection);
  uint32_t change_id = 42;
  mojom_window_tree->SetCapture(change_id, WindowIdToTransportId(window->id()));
  WindowTreeHostImpl* host = window_tree_connection->GetHost(window);
  EXPECT_NE(window, host->GetCaptureWindow());

  // Setting capture after the event is acknowledged should fail
  DispatchEventAndAckImmediately(CreatePointerDownEvent(10, 10));
  mojom_window_tree->SetCapture(++change_id,
                                WindowIdToTransportId(window->id()));
  EXPECT_NE(window, host->GetCaptureWindow());

  // Settings while the event is being process should pass
  DispatchEventWithoutAck(CreatePointerDownEvent(10, 10));
  mojom_window_tree->SetCapture(++change_id,
                                WindowIdToTransportId(window->id()));
  EXPECT_EQ(window, host->GetCaptureWindow());
  AckPreviousEvent();

  // Only the capture window should be able to release capture
  mojom_window_tree->ReleaseCapture(++change_id,
                                    WindowIdToTransportId(root_window->id()));
  EXPECT_EQ(window, host->GetCaptureWindow());
  mojom_window_tree->ReleaseCapture(++change_id,
                                    WindowIdToTransportId(window->id()));
  EXPECT_EQ(nullptr, host->GetCaptureWindow());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
