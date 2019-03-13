// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/ws/window_service_owner.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/ws/event_injector.h"
#include "services/ws/test_window_tree_client.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"

namespace ash {

namespace {

// A testing DragDropDelegate that accepts any drops.
class TestDragDropDelegate : public aura::client::DragDropDelegate {
 public:
  TestDragDropDelegate() = default;
  ~TestDragDropDelegate() override = default;

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}
  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  void OnDragExited() override {}
  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDragDropDelegate);
};

bool IsResizeShadowVisible(ResizeShadow* resize_shadow) {
  if (!resize_shadow)
    return false;
  return resize_shadow->GetLayerForTest()->GetTargetVisibility();
}

}  // namespace

// This test creates a top-level window via the WindowService in SetUp() and
// provides some ease of use functions to access WindowService related state.
class WindowServiceDelegateImplTest : public AshTestBase {
 public:
  WindowServiceDelegateImplTest() = default;
  ~WindowServiceDelegateImplTest() override = default;

  ws::Id GetTopLevelWindowId() {
    return GetWindowTreeTestHelper()->TransportIdForWindow(top_level_.get());
  }

  wm::WmToplevelWindowEventHandler* event_handler() {
    return Shell::Get()
        ->toplevel_window_event_handler()
        ->wm_toplevel_window_event_handler();
  }

  std::vector<ws::Change>* GetWindowTreeClientChanges() {
    return GetTestWindowTreeClient()->tracker()->changes();
  }

  void SetCanAcceptDrops() {
    aura::client::SetDragDropDelegate(top_level_.get(),
                                      &test_drag_drop_delegate_);
  }

  bool IsDragDropInProgress() {
    return aura::client::GetDragDropClient(top_level_->GetRootWindow())
        ->IsDragDropInProgress();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->aura_env()->set_throttle_input_on_resize_for_testing(false);
    NonClientFrameViewAsh::use_empty_minimum_size_for_test_ = true;
    top_level_ = CreateTestWindow(gfx::Rect(100, 100, 100, 100));
    ASSERT_TRUE(top_level_);
    GetEventGenerator()->PressLeftButton();
  }
  void TearDown() override {
    // Ash owns the WindowTree, which also handles deleting |top_level_|. This
    // needs to delete |top_level_| before the WindowTree is deleted, otherwise
    // the WindowTree will delete |top_level_|, leading to a double delete.
    top_level_.reset();
    NonClientFrameViewAsh::use_empty_minimum_size_for_test_ = false;
    AshTestBase::TearDown();
  }

 protected:
  TestDragDropDelegate test_drag_drop_delegate_;
  std::unique_ptr<aura::Window> top_level_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowServiceDelegateImplTest);
};

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      HTCAPTION);
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 6));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  // Releasing the mouse completes the move loop.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=true"));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
}

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveWithMultipleDisplays) {
  UpdateDisplay("500x500,500x500");
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      top_level_->GetBoundsInScreen().origin(), HTCAPTION);
  GetEventGenerator()->MoveMouseTo(gfx::Point(501, 1));
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  EXPECT_EQ(Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "DisplayChanged window_id=0,1 display_id=2200000001"));
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      std::string("BoundsChanged window=0,1 bounds=500,0 100x100 "
                  "local_surface_id=*")));
}

TEST_F(WindowServiceDelegateImplTest, SetWindowBoundsToDifferentDisplay) {
  UpdateDisplay("500x500,500x500");
  EXPECT_EQ(gfx::Point(100, 100), top_level_->GetBoundsInScreen().origin());

  GetWindowTreeClientChanges()->clear();
  GetWindowTreeTestHelper()->window_tree()->SetWindowBounds(
      21, GetTopLevelWindowId(), gfx::Rect(600, 100, 100, 100), base::nullopt);
  EXPECT_EQ(gfx::Point(600, 100), top_level_->GetBoundsInScreen().origin());
  EXPECT_EQ(Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "DisplayChanged window_id=0,1 display_id=2200000001"));
}

TEST_F(WindowServiceDelegateImplTest, DeleteWindowWithInProgressRunLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      29, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      HTCAPTION);
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  top_level_.reset();
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  // Deleting the window implicitly cancels the drag.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=29 success=false"));
}

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveLoopInSecondaryDisplay) {
  UpdateDisplay("500x400,500x400");
  top_level_->SetBoundsInScreen(gfx::Rect(600, 100, 100, 100),
                                GetSecondaryDisplay());

  EXPECT_EQ(Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
            top_level_->GetRootWindow());
  EXPECT_EQ(gfx::Point(600, 100), top_level_->GetBoundsInScreen().origin());

  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      gfx::Point(605, 106), HTCAPTION);

  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(615, 120));
  EXPECT_EQ(gfx::Point(610, 114), top_level_->GetBoundsInScreen().origin());
}

TEST_F(WindowServiceDelegateImplTest, CancelWindowMoveLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      HTCAPTION);
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 6));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
  GetWindowTreeClientChanges()->clear();
  GetWindowTreeTestHelper()->window_tree()->CancelWindowMove(
      GetTopLevelWindowId());
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=false"));
  EXPECT_EQ(gfx::Point(100, 100), top_level_->bounds().origin());
}

TEST_F(WindowServiceDelegateImplTest, WindowResize) {
  gfx::Rect bounds = top_level_->bounds();
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      HTTOPLEFT);
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseBy(5, 6);
  bounds.Inset(5, 6, 0, 0);
  EXPECT_EQ(bounds, top_level_->bounds());
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  // Releasing the mouse completes the move loop.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=true"));
  EXPECT_EQ(bounds, top_level_->bounds());
}

TEST_F(WindowServiceDelegateImplTest, InvalidWindowComponent) {
  gfx::Rect bounds = top_level_->bounds();
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE,
      bounds.origin(), HTCLIENT);
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(5, 6);
  EXPECT_EQ(bounds, top_level_->bounds());
  GetEventGenerator()->ReleaseLeftButton();
}

TEST_F(WindowServiceDelegateImplTest, NestedWindowMoveIsNotAllowed) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      HTCAPTION);
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetWindowTreeClientChanges()->clear();

  // Intentionally invokes PerformWindowMove to make sure it does not break
  // anything.
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      22, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::TOUCH, gfx::Point(),
      HTCAPTION);
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=22 success=false"));

  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=true"));
}

TEST_F(WindowServiceDelegateImplTest, SetWindowResizeShadow) {
  ResizeShadowController* controller = Shell::Get()->resize_shadow_controller();

  GetWindowTreeTestHelper()->window_tree()->SetWindowResizeShadow(
      GetTopLevelWindowId(), HTNOWHERE);
  EXPECT_FALSE(IsResizeShadowVisible(
      controller->GetShadowForWindowForTest(top_level_.get())));

  GetWindowTreeTestHelper()->window_tree()->SetWindowResizeShadow(
      GetTopLevelWindowId(), HTTOPLEFT);
  ResizeShadow* shadow =
      controller->GetShadowForWindowForTest(top_level_.get());
  EXPECT_TRUE(IsResizeShadowVisible(shadow));
  EXPECT_EQ(HTTOPLEFT, shadow->GetLastHitTestForTest());

  // Nothing should change for invalid hit-test.
  GetWindowTreeTestHelper()->window_tree()->SetWindowResizeShadow(
      GetTopLevelWindowId(), HTCLIENT);
  EXPECT_TRUE(IsResizeShadowVisible(shadow));
  EXPECT_EQ(HTTOPLEFT, shadow->GetLastHitTestForTest());

  GetWindowTreeTestHelper()->window_tree()->SetWindowResizeShadow(
      GetTopLevelWindowId(), HTNOWHERE);
  EXPECT_FALSE(IsResizeShadowVisible(shadow));
}

TEST_F(WindowServiceDelegateImplTest, RunDragLoop) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  base::RunLoop run_loop;

  // Post mouse move and release to allow the nested drag loop to pick it up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        ASSERT_TRUE(IsDragDropInProgress());

        // Move mouse to center of |top_level_|.
        GetEventGenerator()->MoveMouseTo(gfx::Point(150, 150));
        GetWindowTreeClientChanges()->clear();
        GetEventGenerator()->ReleaseLeftButton();
      }));

  // Let the drop loop task run.
  run_loop.RunUntilIdle();

  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "OnPerformDragDropCompleted id=21 success=true action=1"));
}

TEST_F(WindowServiceDelegateImplTest, DeleteWindowWithInProgressDragLoop) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  base::RunLoop run_loop;

  // Post the task to delete the window to allow nested drag loop to pick it up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        ASSERT_TRUE(IsDragDropInProgress());

        // Deletes the window.
        top_level_.reset();

        // Move mouse and release the button and it should not crash.
        GetEventGenerator()->MoveMouseTo(gfx::Point(150, 150));
        GetWindowTreeClientChanges()->clear();
        GetEventGenerator()->ReleaseLeftButton();
      }));

  // Let the drop loop task run.
  run_loop.RunUntilIdle();

  // It fails because the target window |top_level_| is deleted.
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      "OnPerformDragDropCompleted id=21 success=false action=0"));
}

TEST_F(WindowServiceDelegateImplTest, CancelDragDropBeforeDragLoopRun) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  // Cancel the drag before the drag loop runs.
  GetWindowTreeTestHelper()->window_tree()->CancelDragDrop(
      GetTopLevelWindowId());

  // Let the drop loop task run.
  base::RunLoop().RunUntilIdle();

  // It fails because the drag is canceled.
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      "OnPerformDragDropCompleted id=21 success=false action=0"));
}

TEST_F(WindowServiceDelegateImplTest, CancelDragDropAfterDragLoopRun) {
  SetCanAcceptDrops();
  GetWindowTreeTestHelper()->window_tree()->PerformDragDrop(
      21, GetTopLevelWindowId(), gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  base::RunLoop run_loop;

  // Post the task to cancel to allow nested drag loop to pick it up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        ASSERT_TRUE(IsDragDropInProgress());

        GetWindowTreeTestHelper()->window_tree()->CancelDragDrop(
            GetTopLevelWindowId());
      }));

  // Let the drop loop task run.
  run_loop.RunUntilIdle();

  // It fails because the drag is canceled.
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      "OnPerformDragDropCompleted id=21 success=false action=0"));
}

TEST_F(WindowServiceDelegateImplTest, ObserveTopmostWindow) {
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(150, 100, 100, 100));
  std::unique_ptr<aura::Window> window3(
      CreateTestWindowInShell(SK_ColorRED, kShellWindowId_DefaultContainer,
                              gfx::Rect(100, 150, 100, 100)));

  // Left button is pressed on SetUp() -- release it first.
  GetEventGenerator()->ReleaseLeftButton();
  GetEventGenerator()->MoveMouseTo(gfx::Point(105, 105));
  GetEventGenerator()->PressLeftButton();
  GetWindowTreeClientChanges()->clear();

  GetWindowTreeTestHelper()->window_tree()->ObserveTopmostWindow(
      ws::mojom::MoveLoopSource::MOUSE, GetTopLevelWindowId());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=null"));
  GetWindowTreeClientChanges()->clear();

  GetEventGenerator()->MoveMouseTo(gfx::Point(155, 105));
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=0,2"));
  GetWindowTreeClientChanges()->clear();

  GetEventGenerator()->MoveMouseTo(gfx::Point(155, 115));
  EXPECT_FALSE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=0,2"));
  GetWindowTreeClientChanges()->clear();

  GetEventGenerator()->MoveMouseTo(gfx::Point(155, 155));
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=null"));
  GetWindowTreeClientChanges()->clear();

  window3.reset();
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=0,2"));
  GetWindowTreeClientChanges()->clear();

  window2->Hide();
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     "TopmostWindowChanged window_id=0,1 window_id2=null"));
  GetWindowTreeClientChanges()->clear();

  GetWindowTreeTestHelper()->window_tree()->StopObservingTopmostWindow();
}

TEST_F(WindowServiceDelegateImplTest, MoveAcrossDisplays) {
  UpdateDisplay("600x400,600+0-400x300");

  GetWindowTreeClientChanges()->clear();

  display::Screen* screen = display::Screen::GetScreen();
  display::Display display1 = screen->GetPrimaryDisplay();
  display::Display display2 = GetSecondaryDisplay();
  EXPECT_EQ(display1.id(),
            screen->GetDisplayNearestWindow(top_level_.get()).id());

  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ws::mojom::MoveLoopSource::MOUSE, gfx::Point(),
      HTCAPTION);
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator()->MoveMouseTo(gfx::Point(610, 6));
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator()->ReleaseLeftButton();

  EXPECT_EQ(display2.id(),
            screen->GetDisplayNearestWindow(top_level_.get()).id());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     std::string("DisplayChanged window_id=0,1 display_id=") +
                         base::NumberToString(display2.id())));
}

TEST_F(WindowServiceDelegateImplTest, RemoveDisplay) {
  UpdateDisplay("500x400,500x400");
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display::Display display2 = GetSecondaryDisplay();

  GetWindowTreeClientChanges()->clear();
  top_level_->SetBoundsInScreen(gfx::Rect(600, 100, 100, 100),
                                GetSecondaryDisplay());
  EXPECT_EQ(Shell::GetRootWindowForDisplayId(display2.id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     std::string("DisplayChanged window_id=0,1 display_id=") +
                         base::NumberToString(display2.id())));

  GetWindowTreeClientChanges()->clear();
  UpdateDisplay("500x400");
  EXPECT_EQ(Shell::GetRootWindowForDisplayId(display1.id()),
            top_level_->GetRootWindow());
  EXPECT_TRUE(
      ContainsChange(*GetWindowTreeClientChanges(),
                     std::string("DisplayChanged window_id=0,1 display_id=") +
                         base::NumberToString(display1.id())));
  EXPECT_TRUE(ContainsChange(
      *GetWindowTreeClientChanges(),
      std::string("BoundsChanged window=0,1 bounds=100,100 100x100 "
                  "local_surface_id=*")));
}

TEST_F(WindowServiceDelegateImplTest, MultiDisplayEventInjector) {
  UpdateDisplay("500x400,500x400");
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();

  top_level_->SetCapture();

  top_level_->SetBoundsInScreen(gfx::Rect(450, 0, 200, 200),
                                GetSecondaryDisplay());
  EXPECT_NE(Shell::GetPrimaryRootWindow(), top_level_->GetRootWindow());

  ws::EventInjector injector(
      Shell::Get()->window_service_owner()->window_service());
  base::RunLoop run_loop;
  ui::test::TestEventHandler handler;
  top_level_->AddPreTargetHandler(&handler);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto ev = std::make_unique<ui::MouseEvent>(
            ui::ET_MOUSE_DRAGGED, gfx::Point(470, 10), gfx::Point(470, 10),
            base::TimeTicks::Now(), 0, 0);
        static_cast<ws::mojom::EventInjector*>(&injector)->InjectEvent(
            display1.id(), std::move(ev),
            base::BindLambdaForTesting([&](bool result) {
              EXPECT_TRUE(result);
              run_loop.Quit();
            }));
      }));
  run_loop.Run();
  EXPECT_EQ(1, handler.num_mouse_events());
  top_level_->RemovePreTargetHandler(&handler);
}

}  // namespace ash
