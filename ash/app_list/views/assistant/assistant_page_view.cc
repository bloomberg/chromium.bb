// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_page_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

constexpr int kHeight = 440;
constexpr int kWidth = 640;

// The height of the search box in |search_result_page_view_|. It is only for
// animation.
constexpr int kSearchBoxHeight = 56;

}  // namespace

AssistantPageView::AssistantPageView(
    ContentsView* contents_view,
    ash::AssistantViewDelegate* assistant_view_delegate)
    : contents_view_(contents_view) {
  assistant_main_view_ = new AssistantMainView(assistant_view_delegate);
  AddChildView(assistant_main_view_);
  InitLayout();
}

AssistantPageView::~AssistantPageView() = default;

void AssistantPageView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          SK_ColorWHITE, search_box::kSearchBoxBorderCornerRadius)));

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void AssistantPageView::Back() {
  contents_view_->Back();
}

const char* AssistantPageView::GetClassName() const {
  return "AssistantPageView";
}

gfx::Size AssistantPageView::CalculatePreferredSize() const {
  return gfx::Size(kWidth, kHeight);
}

void AssistantPageView::RequestFocus() {
  assistant_main_view_->RequestFocus();
}

void AssistantPageView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      // Prevents closing the AppListView when a click event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void AssistantPageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      // Prevents closing the AppListView when a tap event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

gfx::Rect AssistantPageView::GetPageBoundsForState(
    ash::AppListState state) const {
  gfx::Rect onscreen_bounds;

  if (state != ash::AppListState::kStateEmbeddedAssistant) {
    // Hides this view behind the search box by using the same bounds.
    onscreen_bounds =
        AppListPage::contents_view()->GetSearchBoxBoundsForState(state);
  } else {
    onscreen_bounds = AppListPage::GetSearchBoxBounds();
    onscreen_bounds.Offset((onscreen_bounds.width() - kWidth) / 2, 0);
    onscreen_bounds.set_size(GetPreferredSize());
  }

  return onscreen_bounds;
}

gfx::Rect AssistantPageView::GetSearchBoxBounds() const {
  gfx::Rect rect(AppListPage::GetSearchBoxBounds());

  rect.Offset((rect.width() - kWidth) / 2, 0);
  rect.set_size(gfx::Size(kWidth, kSearchBoxHeight));

  return rect;
}

views::View* AssistantPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false, /*dont_loop=*/false);
}

views::View* AssistantPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
}

}  // namespace app_list
