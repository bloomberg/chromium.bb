// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_page_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_web_view.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

// The height of the search box in |search_result_page_view_|. It is only for
// animation.
constexpr int kSearchBoxHeightDip = 56;

// The shadow elevation value for the shadow of the Assistant search box.
constexpr int kShadowElevation = 12;

}  // namespace

AssistantPageView::AssistantPageView(
    ash::AssistantViewDelegate* assistant_view_delegate)
    : assistant_view_delegate_(assistant_view_delegate) {
  InitLayout();

  // |assistant_view_delegate_| could be nullptr in test.
  if (assistant_view_delegate_)
    assistant_view_delegate_->AddUiModelObserver(this);
}

AssistantPageView::~AssistantPageView() {
  if (assistant_view_delegate_)
    assistant_view_delegate_->RemoveUiModelObserver(this);
}

void AssistantPageView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Create and set a shadow to be displayed as a border for this view.
  auto shadow_border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW,
      SK_ColorWHITE);
  shadow_border->SetCornerRadius(
      search_box::kSearchBoxBorderCornerRadiusSearchResult);
  shadow_border->set_md_shadow_elevation(kShadowElevation);
  SetBorder(std::move(shadow_border));

  SetBackground(views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          SK_ColorWHITE, search_box::kSearchBoxBorderCornerRadiusSearchResult,
          border()->GetInsets())));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (assistant_view_delegate_) {
    assistant_main_view_ = new AssistantMainView(assistant_view_delegate_);
    AddChildView(assistant_main_view_);

    // Web view.
    assistant_web_view_ = new ash::AssistantWebView(assistant_view_delegate_);
    AddChildView(assistant_web_view_);

    // Update the view state based on the current UI mode.
    OnUiModeChanged(assistant_view_delegate_->GetUiModel()->ui_mode());
  }
}

const char* AssistantPageView::GetClassName() const {
  return "AssistantPageView";
}

gfx::Size AssistantPageView::CalculatePreferredSize() const {
  return gfx::Size(ash::kPreferredWidthDip, ash::kMaxHeightEmbeddedDip);
}

void AssistantPageView::RequestFocus() {
  if (!assistant_view_delegate_)
    return;

  switch (assistant_view_delegate_->GetUiModel()->ui_mode()) {
    case ash::AssistantUiMode::kLauncherEmbeddedUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case ash::AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->RequestFocus();
      break;
    case ash::AssistantUiMode::kMainUi:
    case ash::AssistantUiMode::kMiniUi:
      NOTREACHED();
      break;
  }
}

void AssistantPageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW));
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
  gfx::Rect bounds;
  if (state != ash::AppListState::kStateEmbeddedAssistant) {
    // Hides this view behind the search box by using the same bounds.
    bounds = AppListPage::contents_view()->GetSearchBoxBoundsForState(state);
  } else {
    bounds = AppListPage::GetSearchBoxBounds();
    bounds.Offset((bounds.width() - ash::kPreferredWidthDip) / 2, 0);
    bounds.set_size(GetPreferredSize());
  }

  return AddShadowBorderToBounds(bounds);
}

gfx::Rect AssistantPageView::GetSearchBoxBounds() const {
  gfx::Rect bounds(AppListPage::GetSearchBoxBounds());

  bounds.Offset((bounds.width() - ash::kPreferredWidthDip) / 2, 0);
  bounds.set_size(gfx::Size(ash::kPreferredWidthDip, kSearchBoxHeightDip));

  return bounds;
}

views::View* AssistantPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false, /*dont_loop=*/false);
}

views::View* AssistantPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
}

void AssistantPageView::OnUiModeChanged(ash::AssistantUiMode ui_mode) {
  for (int i = 0; i < child_count(); ++i)
    child_at(i)->SetVisible(false);

  switch (ui_mode) {
    case ash::AssistantUiMode::kLauncherEmbeddedUi:
      if (assistant_main_view_)
        assistant_main_view_->SetVisible(true);
      break;
    case ash::AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->SetVisible(true);
      break;
    case ash::AssistantUiMode::kMainUi:
    case ash::AssistantUiMode::kMiniUi:
      NOTREACHED();
      break;
  }

  PreferredSizeChanged();
  RequestFocus();
}

void AssistantPageView::OnUiVisibilityChanged(
    ash::AssistantVisibility new_visibility,
    ash::AssistantVisibility old_visibility,
    base::Optional<ash::AssistantEntryPoint> entry_point,
    base::Optional<ash::AssistantExitPoint> exit_point) {
  if (!assistant_view_delegate_)
    return;

  if (new_visibility != ash::AssistantVisibility::kVisible)
    return;

  const bool prefer_voice = assistant_view_delegate_->IsTabletMode() ||
                            assistant_view_delegate_->IsLaunchWithMicOpen();
  if (!ash::assistant::util::IsVoiceEntryPoint(entry_point.value(),
                                               prefer_voice)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

gfx::Rect AssistantPageView::AddShadowBorderToBounds(
    const gfx::Rect& bounds) const {
  gfx::Rect new_bounds(bounds);
  new_bounds.Inset(-border()->GetInsets());
  return new_bounds;
}

}  // namespace app_list
