// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shelf_view_test_api.h"

#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_view.h"
#include "base/message_loop/message_loop.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view_model.h"

namespace {

// A class used to wait for animations.
class TestAPIAnimationObserver : public views::BoundsAnimatorObserver {
 public:
  TestAPIAnimationObserver() {}
  virtual ~TestAPIAnimationObserver() {}

  // views::BoundsAnimatorObserver overrides:
  virtual void OnBoundsAnimatorProgressed(
      views::BoundsAnimator* animator) OVERRIDE {}
  virtual void OnBoundsAnimatorDone(views::BoundsAnimator* animator) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAPIAnimationObserver);
};

}  // namespace

namespace ash {
namespace test {

ShelfViewTestAPI::ShelfViewTestAPI(ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

ShelfViewTestAPI::~ShelfViewTestAPI() {
}

int ShelfViewTestAPI::GetButtonCount() {
  return shelf_view_->view_model_->view_size();
}

ShelfButton* ShelfViewTestAPI::GetButton(int index) {
  // App list button is not a ShelfButton.
  if (shelf_view_->model_->items()[index].type == ash::TYPE_APP_LIST)
    return NULL;

  return static_cast<ShelfButton*>(shelf_view_->view_model_->view_at(index));
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
  if (!shelf_view_->IsShowingOverflowBubble())
    shelf_view_->ToggleOverflowBubble();
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

  scoped_ptr<TestAPIAnimationObserver> observer(new TestAPIAnimationObserver());
  shelf_view_->bounds_animator_->AddObserver(observer.get());

  // This nested loop will quit when TestAPIAnimationObserver's
  // OnBoundsAnimatorDone is called.
  base::MessageLoop::current()->Run();

  shelf_view_->bounds_animator_->RemoveObserver(observer.get());
}

OverflowBubble* ShelfViewTestAPI::overflow_bubble() {
  return shelf_view_->overflow_bubble_.get();
}

gfx::Size ShelfViewTestAPI::GetPreferredSize() {
  return shelf_view_->GetPreferredSize();
}

int ShelfViewTestAPI::GetButtonSize() {
  return kShelfButtonSize;
}

int ShelfViewTestAPI::GetButtonSpacing() {
  return kShelfButtonSpacing;
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

}  // namespace test
}  // namespace ash
