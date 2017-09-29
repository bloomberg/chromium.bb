// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace ash {
namespace {

Shelf* GetShelfForDisplay(int64_t display_id) {
  return Shell::GetRootWindowControllerWithDisplayId(display_id)->shelf();
}

// A test implementation of the ShelfObserver mojo interface.
class TestShelfObserver : public mojom::ShelfObserver {
 public:
  TestShelfObserver() = default;
  ~TestShelfObserver() override = default;

  // mojom::ShelfObserver:
  void OnShelfItemAdded(int32_t, const ShelfItem&) override { added_count_++; }
  void OnShelfItemRemoved(const ShelfID&) override { removed_count_++; }
  void OnShelfItemMoved(const ShelfID&, int32_t) override {}
  void OnShelfItemUpdated(const ShelfItem&) override {}
  void OnShelfItemDelegateChanged(const ShelfID&,
                                  mojom::ShelfItemDelegatePtr) override {}

  size_t added_count() const { return added_count_; }
  size_t removed_count() const { return removed_count_; }

 private:
  size_t added_count_ = 0;
  size_t removed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfObserver);
};

using ShelfControllerTest = AshTestBase;

TEST_F(ShelfControllerTest, IntializesAppListItemDelegate) {
  ShelfModel* model = Shell::Get()->shelf_controller()->model();
  EXPECT_EQ(1, model->item_count());
  EXPECT_EQ(kAppListId, model->items()[0].id.app_id);
  EXPECT_TRUE(model->GetShelfItemDelegate(ShelfID(kAppListId)));
}

TEST_F(ShelfControllerTest, ShelfModelChangesWithoutSync) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  if (controller->should_synchronize_shelf_models())
    return;

  TestShelfObserver observer;
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::ShelfObserver> binding(
      &observer, mojo::MakeIsolatedRequest(&observer_ptr));
  controller->AddObserver(observer_ptr.PassInterface());

  // The ShelfModel should be initialized with a single item for the AppList.
  // Without syncing, the observer should not be notified of ShelfModel changes.
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(0u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Add a ShelfModel item; |observer| should not be notified without sync.
  ShelfItem item;
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("foo");
  int index = controller->model()->Add(item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller->model()->item_count());
  EXPECT_EQ(0u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Remove a ShelfModel item; |observer| should not be notified without sync.
  controller->model()->RemoveItemAt(index);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(0u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());
}

TEST_F(ShelfControllerTest, ShelfModelChangesWithSync) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  if (!controller->should_synchronize_shelf_models())
    return;

  TestShelfObserver observer;
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::ShelfObserver> binding(
      &observer, mojo::MakeIsolatedRequest(&observer_ptr));
  controller->AddObserver(observer_ptr.PassInterface());
  base::RunLoop().RunUntilIdle();

  // The ShelfModel should be initialized with a single item for the AppList.
  // When syncing, the observer is immediately notified of existing shelf items.
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(1u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Add a ShelfModel item; |observer| should be notified when syncing.
  ShelfItem item;
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("foo");
  int index = controller->model()->Add(item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller->model()->item_count());
  EXPECT_EQ(2u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Remove a ShelfModel item; |observer| should be notified when syncing.
  controller->model()->RemoveItemAt(index);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(2u, observer.added_count());
  EXPECT_EQ(1u, observer.removed_count());

  // Simulate adding an item remotely; Ash should apply the change.
  // |observer| is not notified; see mojom::ShelfController for rationale.
  controller->AddShelfItem(index, item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller->model()->item_count());
  EXPECT_EQ(2u, observer.added_count());
  EXPECT_EQ(1u, observer.removed_count());

  // Simulate removing an item remotely; Ash should apply the change.
  // |observer| is not notified; see mojom::ShelfController for rationale.
  controller->RemoveShelfItem(item.id);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(2u, observer.added_count());
  EXPECT_EQ(1u, observer.removed_count());
}

class ShelfControllerPrefsTest : public AshTestBase {
 public:
  ShelfControllerPrefsTest() = default;
  ~ShelfControllerPrefsTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfControllerPrefsTest);
};

// Ensure shelf settings are updated on preference changes.
TEST_F(ShelfControllerPrefsTest, ShelfRespectsPrefs) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetString(prefs::kShelfAlignmentLocal, "Left");
  prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, "Always");

  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
}

// Ensure shelf settings are updated on per-display preference changes.
TEST_F(ShelfControllerPrefsTest, ShelfRespectsPerDisplayPrefs) {
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t id1 = GetPrimaryDisplay().id();
  const int64_t id2 = GetSecondaryDisplay().id();
  Shelf* shelf1 = GetShelfForDisplay(id1);
  Shelf* shelf2 = GetShelfForDisplay(id2);

  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf1->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf2->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf1->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf2->auto_hide_behavior());

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, id1, SHELF_ALIGNMENT_LEFT);
  SetShelfAlignmentPref(prefs, id2, SHELF_ALIGNMENT_RIGHT);
  SetShelfAutoHideBehaviorPref(prefs, id1, SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  SetShelfAutoHideBehaviorPref(prefs, id2, SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf1->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, shelf2->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf1->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf2->auto_hide_behavior());
}

// Ensure shelf settings are correct after display swap, see crbug.com/748291
TEST_F(ShelfControllerPrefsTest, ShelfSettingsValidAfterDisplaySwap) {
  // Simulate adding an external display at the lock screen.
  GetSessionControllerClient()->RequestLockScreen();
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t internal_display_id = GetPrimaryDisplay().id();
  const int64_t external_display_id = GetSecondaryDisplay().id();

  // The primary shelf should be on the internal display.
  EXPECT_EQ(GetPrimaryShelf(), GetShelfForDisplay(internal_display_id));
  EXPECT_NE(GetPrimaryShelf(), GetShelfForDisplay(external_display_id));

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Check for the default shelf preferences.
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfAutoHideBehaviorPref(prefs, internal_display_id));
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfAutoHideBehaviorPref(prefs, external_display_id));
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM,
            GetShelfAlignmentPref(prefs, internal_display_id));
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM,
            GetShelfAlignmentPref(prefs, external_display_id));

  // Check the current state; shelves have locked alignments in the lock screen.
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());

  // Set some shelf prefs to differentiate the two shelves, check state.
  SetShelfAlignmentPref(prefs, internal_display_id, SHELF_ALIGNMENT_LEFT);
  SetShelfAlignmentPref(prefs, external_display_id, SHELF_ALIGNMENT_RIGHT);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());

  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());

  // Simulate the external display becoming the primary display. The shelves are
  // swapped (each instance now has a different display id), check state.
  SwapPrimaryDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());

  // After screen unlock the shelves should have the expected alignment values.
  GetSessionControllerClient()->UnlockScreen();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
}

TEST_F(ShelfControllerPrefsTest, ShelfSettingsInTabletMode) {
  Shelf* shelf = GetPrimaryShelf();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, GetPrimaryDisplay().id(), SHELF_ALIGNMENT_LEFT);
  SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplay().id(),
                               SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  ASSERT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  ASSERT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Verify after entering tablet mode, the shelf alignment is bottom and the
  // auto hide behavior is never.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Verify after exiting tablet mode, the shelf alignment and auto hide
  // behavior get their stored pref values.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
}

}  // namespace
}  // namespace ash
