// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shelf_view_test_api.h"

#include "ash/common/shelf/overflow_button.h"
#include "ash/common/shelf/shelf_button.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_view.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_model.h"

namespace {

// A class used to wait for animations.
class TestAPIAnimationObserver : public views::BoundsAnimatorObserver {
 public:
  TestAPIAnimationObserver() {}
  ~TestAPIAnimationObserver() override {}

  // views::BoundsAnimatorObserver overrides:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAPIAnimationObserver);
};

}  // namespace

namespace ash {
namespace test {

ShelfViewTestAPI::ShelfViewTestAPI(ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

ShelfViewTestAPI::~ShelfViewTestAPI() {}

int ShelfViewTestAPI::GetButtonCount() {
  return shelf_view_->view_model_->view_size();
}

ShelfButton* ShelfViewTestAPI::GetButton(int index) {
  // App list button is not a ShelfButton.
  if (shelf_view_->model_->items()[index].type == ash::TYPE_APP_LIST)
    return nullptr;

  return static_cast<ShelfButton*>(GetViewAt(index));
}

views::View* ShelfViewTestAPI::GetViewAt(int index) {
  return shelf_view_->view_model_->view_at(index);
}

int ShelfViewTestAPI::GetFirstVisibleIndex() {
  return shelf_view_->first_visible_index_;
}

int ShelfViewTestAPI::GetLastVisibleIndex() {
  return shelf_view_->last_visible_index_;
}

bool ShelfViewTestAPI::IsOverflowButtonVisible() {
  return shelf_view_->overflow_button_->visible();
}

void ShelfViewTestAPI::ShowOverflowBubble() {
  DCHECK(!shelf_view_->IsShowingOverflowBubble());
  shelf_view_->ToggleOverflowBubble();
}

void ShelfViewTestAPI::HideOverflowBubble() {
  DCHECK(shelf_view_->IsShowingOverflowBubble());
  shelf_view_->ToggleOverflowBubble();
}

bool ShelfViewTestAPI::IsShowingOverflowBubble() const {
  return shelf_view_->IsShowingOverflowBubble();
}

const gfx::Rect& ShelfViewTestAPI::GetBoundsByIndex(int index) {
  return shelf_view_->view_model_->view_at(index)->bounds();
}

const gfx::Rect& ShelfViewTestAPI::GetIdealBoundsByIndex(int index) {
  return shelf_view_->view_model_->ideal_bounds(index);
}

void ShelfViewTestAPI::SetAnimationDuration(int duration_ms) {
  shelf_view_->bounds_animator_->SetAnimationDuration(duration_ms);
}

void ShelfViewTestAPI::RunMessageLoopUntilAnimationsDone() {
  if (!shelf_view_->bounds_animator_->IsAnimating())
    return;

  std::unique_ptr<TestAPIAnimationObserver> observer(
      new TestAPIAnimationObserver());
  shelf_view_->bounds_animator_->AddObserver(observer.get());

  // This nested loop will quit when TestAPIAnimationObserver's
  // OnBoundsAnimatorDone is called.
  base::RunLoop().Run();

  shelf_view_->bounds_animator_->RemoveObserver(observer.get());
}

void ShelfViewTestAPI::CloseMenu() {
  if (!shelf_view_->launcher_menu_runner_)
    return;

  shelf_view_->launcher_menu_runner_->Cancel();
}

OverflowBubble* ShelfViewTestAPI::overflow_bubble() {
  return shelf_view_->overflow_bubble_.get();
}

OverflowButton* ShelfViewTestAPI::overflow_button() const {
  return shelf_view_->overflow_button_;
}

ShelfTooltipManager* ShelfViewTestAPI::tooltip_manager() {
  return &shelf_view_->tooltip_;
}

gfx::Size ShelfViewTestAPI::GetPreferredSize() {
  return shelf_view_->GetPreferredSize();
}

int ShelfViewTestAPI::GetMinimumDragDistance() const {
  return ShelfView::kMinimumDragDistance;
}

void ShelfViewTestAPI::ButtonPressed(views::Button* sender,
                                     const ui::Event& event,
                                     views::InkDrop* ink_drop) {
  return shelf_view_->ButtonPressed(sender, event, ink_drop);
}

bool ShelfViewTestAPI::SameDragType(ShelfItemType typea,
                                    ShelfItemType typeb) const {
  return shelf_view_->SameDragType(typea, typeb);
}

void ShelfViewTestAPI::SetShelfDelegate(ShelfDelegate* delegate) {
  shelf_view_->delegate_ = delegate;
}

gfx::Rect ShelfViewTestAPI::GetBoundsForDragInsertInScreen() {
  return shelf_view_->GetBoundsForDragInsertInScreen();
}

bool ShelfViewTestAPI::IsRippedOffFromShelf() {
  return shelf_view_->dragged_off_shelf_;
}

bool ShelfViewTestAPI::DraggedItemFromOverflowToShelf() {
  return shelf_view_->dragged_off_from_overflow_to_shelf_;
}

ShelfButtonPressedMetricTracker*
ShelfViewTestAPI::shelf_button_pressed_metric_tracker() {
  return &(shelf_view_->shelf_button_pressed_metric_tracker_);
}

}  // namespace test
}  // namespace ash
