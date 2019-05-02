// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

std::unique_ptr<aura::Window> CreateTransientWindow(
    aura::Window* transient_parent,
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window =
      window_factory::NewWindow(nullptr, aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  ::wm::AddTransientChild(transient_parent, window.get());
  aura::client::ParentWindowWithContext(
      window.get(), transient_parent->GetRootWindow(), bounds);
  window->Show();
  return window;
}

bool DoesActiveDeskContainWindow(aura::Window* window) {
  return DesksController::Get()->active_desk()->windows().contains(window);
}

OverviewGrid* GetOverviewGridForRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->IsSelecting());

  return overview_controller->overview_session()->GetGridWithRootWindow(root);
}

void CloseDeskFromMiniView(const DeskMiniView* desk_mini_view,
                           ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  // Move to the center of the mini view so that the close button shows up.
  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(desk_mini_view->close_desk_button()->visible());
  // Move to the center of the close button and click.
  event_generator->MoveMouseTo(
      desk_mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

// Defines an observer to test DesksController notifications.
class TestObserver : public DesksController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  const std::vector<const Desk*>& desks() const { return desks_; }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {
    desks_.emplace_back(desk);
    EXPECT_TRUE(DesksController::Get()->are_desks_being_modified());
  }
  void OnDeskRemoved(const Desk* desk) override {
    base::Erase(desks_, desk);
    EXPECT_TRUE(DesksController::Get()->are_desks_being_modified());
  }
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    EXPECT_TRUE(DesksController::Get()->are_desks_being_modified());
  }

 private:
  std::vector<const Desk*> desks_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class DesksTest : public AshTestBase {
 public:
  DesksTest() = default;
  ~DesksTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DesksTest);
};

TEST_F(DesksTest, DesksCreationAndRemoval) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  // There's always a default pre-existing desk that cannot be removed.
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());

  // Add desks until no longer possible.
  while (controller->CanCreateDesks())
    controller->NewDesk();

  // Expect we've reached the max number of desks, and we've been notified only
  // with the newly created desks.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, observer.desks().size());
  EXPECT_TRUE(controller->CanRemoveDesks());

  // Remove all desks until no longer possible, and expect that there's always
  // one default desk remaining.
  while (controller->CanRemoveDesks())
    controller->RemoveDesk(observer.desks().back());

  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(observer.desks().empty());

  controller->RemoveObserver(&observer);
}

TEST_F(DesksTest, DesksBarViewDeskCreation) {
  TestObserver observer;
  auto* controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  // Initially the grid is not offset down when there are no desk mini_views
  // once animations are added.
  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());

  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();

  // Since we have a single default desk, there should be no mini_views, and the
  // new desk button is enabled.
  DCHECK(desks_bar_view);
  EXPECT_TRUE(desks_bar_view->mini_views().empty());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->enabled());

  // Click many times on the new desk button and expect only the max number of
  // desks will be created, and the button is no longer enabled.
  const gfx::Point button_center =
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(button_center);
  for (size_t i = 0; i < desks_util::kMaxNumberOfDesks + 2; ++i)
    event_generator->ClickLeftButton();

  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_TRUE(controller->CanRemoveDesks());
  EXPECT_FALSE(desks_bar_view->new_desk_button()->enabled());

  // Hover over one of the mini_views, and expect that the close button becomes
  // visible.
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_FALSE(mini_view->close_desk_button()->visible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(mini_view->close_desk_button()->visible());

  // Use the close button to close the desk.
  event_generator->MoveMouseTo(
      mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // The new desk button is now enabled again.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->enabled());

  // Exit overview mode and re-enter. Since we have more than one pre-existing
  // desks, their mini_views should be created upon construction of the desks
  // bar.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->IsSelecting());
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());

  // Get the new grid and the new desk_bar_view.
  overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  desks_bar_view = overview_grid->GetDesksBarViewForTesting();

  DCHECK(desks_bar_view);
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->enabled());
}

TEST_F(DesksTest, DeskActivation) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desk_1->GetDeskContainerForRoot(root));

  // Create three new desks, and activate the one in the middle.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->are_desks_being_modified());
  controller->ActivateDesk(desk_2);
  EXPECT_FALSE(controller->are_desks_being_modified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, which is in the middle, activation should move to
  // the left, so desk 1 should be activated.
  EXPECT_FALSE(controller->are_desks_being_modified());
  controller->RemoveDesk(desk_2);
  EXPECT_FALSE(controller->are_desks_being_modified());
  ASSERT_EQ(3u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, it's the first one on the left, so desk_3 (on the
  // right) will be activated.
  EXPECT_FALSE(controller->are_desks_being_modified());
  controller->RemoveDesk(desk_1);
  EXPECT_FALSE(controller->are_desks_being_modified());
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_EQ(desk_3, controller->active_desk());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());
}

TEST_F(DesksTest, TransientWindows) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create two windows, one is a transient child of the other.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientWindow(win0.get(), gfx::Rect(100, 100, 100, 100));

  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win0.get()));
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win1.get()));

  // Create a new desk and activate it.
  controller->NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_2->windows().empty());
  controller->ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());

  // Create another transient child of the earlier transient child, and confirm
  // it's tracked in desk_1 (even though desk_2 is the currently active one).
  // This is because the transient parent exists in desk_1.
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 50, 50));
  ::wm::AddTransientChild(win1.get(), win2.get());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));

  // Remove the inactive desk 1, and expect that its windows, including
  // transient will move to desk 2.
  controller->RemoveDesk(desk_1);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));
}

TEST_F(DesksTest, WindowActivation) {
  // Create three windows.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateTestWindow(gfx::Rect(100, 100, 100, 100));

  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));

  // Activate win0 and expects that it remains activated until we switch desks.
  wm::ActivateWindow(win0.get());

  // Create a new desk and activate it. Expect it's not tracking any windows
  // yet.
  auto* controller = DesksController::Get();
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  // Activate the newly-added desk. Expect that the tracked windows per each
  // desk will remain the same.
  controller->ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  // `desk_2` has no windows, so now no window should be active, and windows on
  // `desk_1` cannot be activated.
  EXPECT_EQ(nullptr, wm::GetActiveWindow());
  EXPECT_FALSE(wm::CanActivateWindow(win0.get()));
  EXPECT_FALSE(wm::CanActivateWindow(win1.get()));
  EXPECT_FALSE(wm::CanActivateWindow(win2.get()));

  // Create two new windows, they should now go to desk_2.
  auto win3 = CreateTestWindow(gfx::Rect(0, 0, 300, 200));
  auto win4 = CreateTestWindow(gfx::Rect(10, 30, 400, 200));
  wm::ActivateWindow(win3.get());
  EXPECT_EQ(2u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  EXPECT_EQ(win3.get(), wm::GetActiveWindow());

  // Delete `win0` and expect that `desk_1`'s windows will be updated.
  win0.reset();
  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_EQ(2u, desk_2->windows().size());
  // No change in the activation.
  EXPECT_EQ(win3.get(), wm::GetActiveWindow());

  // Switch back to `desk_1`. Now we can activate its windows.
  controller->ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));
  EXPECT_FALSE(wm::CanActivateWindow(win3.get()));
  EXPECT_FALSE(wm::CanActivateWindow(win4.get()));

  // After `win0` has been deleted, `win2` is next on the MRU list.
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());

  // Remove `desk_2` and expect that its windows will be moved to the active
  // desk.
  controller->RemoveDesk(desk_2);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(4u, desk_1->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));

  // `desk_2`'s windows moved to `desk_1`, but that should not change the
  // already active window.
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());

  // Moved windows can now be activated.
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));
}

TEST_F(DesksTest, ActivateDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), wm::GetActiveWindow());

  // Enter overview mode, and expect the desk bar is shown with exactly four
  // desks mini views, and there are exactly two windows in the overview mode
  // grid.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  EXPECT_EQ(2u, overview_grid->window_list().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_4, mini_view->desk());
  EXPECT_FALSE(mini_view->close_desk_button()->visible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  event_generator->ClickLeftButton();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->IsSelecting());
  // Exiting overview mode should not restore focus to a window on a
  // now-inactive desk. Run a loop since the overview session is destroyed async
  // and until that happens, focus will be on the dummy
  // "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, wm::GetActiveWindow());

  // Create one window in desk_4 and enter overview mode. Expect the grid is
  // showing exactly one window.
  auto win2 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());

  // When exiting overview mode without changing desks, the focus should be
  // restored to the same window.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->IsSelecting());
  // Run a loop since the overview session is destroyed async and until that
  // happens, focus will be on the dummy "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());
}

TEST_F(DesksTest, RemoveInactiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  controller->NewDesk();
  controller->NewDesk();
  controller->NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  // Active desk_4 and enter overview mode. Expect that the grid is currently
  // empty.
  const Desk* desk_4 = controller->desks()[3].get();
  controller->ActivateDesk(desk_4);
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->window_list().empty());

  // Remove desk_1 using the close button on its mini view. desk_1 is currently
  // inactive. Its windows should be moved to desk_4 and added to the overview
  // grid in the MRU order (win0, and win1).
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const auto* mini_view = desks_bar_view->mini_views().front().get();
  EXPECT_EQ(desk_1, mini_view->desk());
  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->IsSelecting());
  ASSERT_EQ(2u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[1].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->IsSelecting());
}

TEST_F(DesksTest, RemoveActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one desk other than the default initial desk.
  controller->NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateTestWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), wm::GetActiveWindow());

  // Activate desk_2 and create one more window.
  const Desk* desk_2 = controller->desks()[1].get();
  controller->ActivateDesk(desk_2);
  auto win2 = CreateTestWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  EXPECT_EQ(win2.get(), wm::GetActiveWindow());

  // Enter overview mode, and remove desk_2 from its mini-view close button.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());
  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_2, mini_view->desk());
  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  // desk_1 will become active, and windows from desk_2 and desk_1 will merge
  // and added in the overview grid in the order of MRU.
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_EQ(1u, desks_bar_view->mini_views().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(overview_controller->IsSelecting());
  EXPECT_EQ(3u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));

  // The MRU order is {win2, win0, win1}.
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[2].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->IsSelecting());
}

// TODO(afakhry): Add more tests:
// - Multi displays.
// - Always on top windows are not tracked by any desk.
// - Reusing containers when desks are removed and created.

}  // namespace

}  // namespace ash
