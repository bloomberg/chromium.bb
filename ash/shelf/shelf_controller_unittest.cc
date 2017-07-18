// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include <set>
#include <string>

#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/display/types/display_constants.h"

namespace ash {
namespace {

// A test implementation of the ShelfObserver mojo interface.
class TestShelfObserver : public mojom::ShelfObserver {
 public:
  TestShelfObserver() = default;
  ~TestShelfObserver() override = default;

  // mojom::ShelfObserver:
  void OnShelfInitialized(int64_t display_id) override {
    display_id_ = display_id;
  }
  void OnAlignmentChanged(ShelfAlignment alignment,
                          int64_t display_id) override {
    alignment_ = alignment;
    display_id_ = display_id;
  }
  void OnAutoHideBehaviorChanged(ShelfAutoHideBehavior auto_hide,
                                 int64_t display_id) override {
    auto_hide_ = auto_hide;
    display_id_ = display_id;
  }
  void OnShelfItemAdded(int32_t, const ShelfItem&) override { added_count_++; }
  void OnShelfItemRemoved(const ShelfID&) override { removed_count_++; }
  void OnShelfItemMoved(const ShelfID&, int32_t) override {}
  void OnShelfItemUpdated(const ShelfItem&) override {}
  void OnShelfItemDelegateChanged(const ShelfID&,
                                  mojom::ShelfItemDelegatePtr) override {}

  int64_t display_id() const { return display_id_; }
  ShelfAlignment alignment() const { return alignment_; }
  ShelfAutoHideBehavior auto_hide() const { return auto_hide_; }
  size_t added_count() const { return added_count_; }
  size_t removed_count() const { return removed_count_; }

 private:
  int64_t display_id_ = display::kInvalidDisplayId;
  ShelfAlignment alignment_ = SHELF_ALIGNMENT_BOTTOM_LOCKED;
  ShelfAutoHideBehavior auto_hide_ = SHELF_AUTO_HIDE_ALWAYS_HIDDEN;
  size_t added_count_ = 0;
  size_t removed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfObserver);
};

using ShelfControllerTest = AshTestBase;
using NoSessionShelfControllerTest = NoSessionAshTestBase;

TEST_F(ShelfControllerTest, IntializesAppListItemDelegate) {
  ShelfModel* model = Shell::Get()->shelf_controller()->model();
  EXPECT_EQ(1, model->item_count());
  EXPECT_EQ(kAppListId, model->items()[0].id.app_id);
  EXPECT_TRUE(model->GetShelfItemDelegate(ShelfID(kAppListId)));
}

TEST_F(NoSessionShelfControllerTest, AlignmentAndAutoHide) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  TestShelfObserver observer;
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::ShelfObserver> binding(
      &observer, mojo::MakeIsolatedRequest(&observer_ptr));
  controller->AddObserver(observer_ptr.PassInterface());

  // Simulated login should initialize the primary shelf and notify |observer|.
  EXPECT_EQ(display::kInvalidDisplayId, observer.display_id());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED, observer.alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_ALWAYS_HIDDEN, observer.auto_hide());
  SetUserLoggedIn(true);
  SetSessionStarted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPrimaryDisplay().id(), observer.display_id());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, observer.alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, observer.auto_hide());

  // Changing shelf properties should notify |observer|.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, observer.alignment());
  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, observer.auto_hide());
}

TEST_F(ShelfControllerTest, ShelfModelChangesInClassicAsh) {
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  ShelfController* controller = Shell::Get()->shelf_controller();
  TestShelfObserver observer;
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::ShelfObserver> binding(
      &observer, mojo::MakeIsolatedRequest(&observer_ptr));
  controller->AddObserver(observer_ptr.PassInterface());

  // The ShelfModel should be initialized with a single item for the AppList.
  // In classic ash, the observer should not be notified of ShelfModel changes.
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(0u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Add a ShelfModel item; |observer| should not be notified in classic ash.
  ShelfItem item;
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("foo");
  int index = controller->model()->Add(item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller->model()->item_count());
  EXPECT_EQ(0u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Remove a ShelfModel item; |observer| should not be notified in classic ash.
  controller->model()->RemoveItemAt(index);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(0u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());
}

TEST_F(ShelfControllerTest, ShelfModelChangesInMash) {
  if (Shell::GetAshConfig() != Config::MASH)
    return;

  ShelfController* controller = Shell::Get()->shelf_controller();
  TestShelfObserver observer;
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::ShelfObserver> binding(
      &observer, mojo::MakeIsolatedRequest(&observer_ptr));
  controller->AddObserver(observer_ptr.PassInterface());
  base::RunLoop().RunUntilIdle();

  // The ShelfModel should be initialized with a single item for the AppList.
  // In mash, the observer is immediately notified of existing shelf items.
  EXPECT_EQ(1, controller->model()->item_count());
  EXPECT_EQ(1u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Add a ShelfModel item; |observer| should be notified in mash.
  ShelfItem item;
  item.type = TYPE_PINNED_APP;
  item.id = ShelfID("foo");
  int index = controller->model()->Add(item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller->model()->item_count());
  EXPECT_EQ(2u, observer.added_count());
  EXPECT_EQ(0u, observer.removed_count());

  // Remove a ShelfModel item; |observer| should be notified in mash.
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

}  // namespace
}  // namespace ash
