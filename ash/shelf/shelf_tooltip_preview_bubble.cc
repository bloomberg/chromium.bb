// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_preview_bubble.h"

#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_preview_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {


// The padding inside the tooltip.
constexpr int kTooltipPaddingTop = 8;
constexpr int kTooltipPaddingBottom = 16;
constexpr int kTooltipPaddingLeftRight = 16;

// The padding between individual previews.
constexpr int kPreviewPadding = 10;

// The border radius of the whole bubble
constexpr int kPreviewBubbleBorderRadius = 16;

// The margin between the bubble and the shelf.
constexpr int kDistanceToShelf = 8;

ShelfTooltipPreviewBubble::ShelfTooltipPreviewBubble(
    views::View* anchor,
    const std::vector<aura::Window*>& windows,
    ShelfTooltipManager* manager,
    ShelfAlignment alignment,
    SkColor background_color)
    : ShelfBubble(anchor, alignment, background_color),
      manager_(manager),
      shelf_alignment_(alignment) {
  set_border_radius(kPreviewBubbleBorderRadius);
  // The parent class sets non-zero margins. Reset them to zero.
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(kTooltipPaddingTop, kTooltipPaddingLeftRight,
                  kTooltipPaddingBottom, kTooltipPaddingLeftRight),
      kPreviewPadding));

  const ui::NativeTheme* theme = anchor_widget()->GetNativeTheme();

  for (auto* window : windows) {
    WindowPreview* preview = new WindowPreview(window, this, theme);
    AddChildView(preview);
    previews_.push_back(preview);
  }

  CreateBubble();
  PipPositioner::MarkWindowAsIgnoredForCollisionDetection(
      GetWidget()->GetNativeWindow());
}

ShelfTooltipPreviewBubble::~ShelfTooltipPreviewBubble() = default;

void ShelfTooltipPreviewBubble::RemovePreview(WindowPreview* to_remove) {
  base::Erase(previews_, to_remove);
  RemoveChildView(to_remove);
  // If we don't have any previews left, close the tooltip.
  if (previews_.empty()) {
    manager_->Close();
  }
}

gfx::Rect ShelfTooltipPreviewBubble::GetBubbleBounds() {
  // TODO(manucornet): Find out why |set_arrow_offset| doesn't work for the
  // same purpose. This would allow us to remove this method and the
  // |shelf_alignment_| field.
  gfx::Rect bounds = BubbleDialogDelegateView::GetBubbleBounds();
  if (shelf_alignment_ == SHELF_ALIGNMENT_BOTTOM ||
      shelf_alignment_ == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    bounds.set_y(bounds.y() - kDistanceToShelf);
  } else {
    bounds.set_x(bounds.x() - kDistanceToShelf);
  }
  return bounds;
}

bool ShelfTooltipPreviewBubble::ShouldCloseOnPressDown() {
  return false;
}

bool ShelfTooltipPreviewBubble::ShouldCloseOnMouseExit() {
  return false;
}

float ShelfTooltipPreviewBubble::GetMaxPreviewRatio() const {
  float max_ratio = kShelfTooltipPreviewMinRatio;
  for (WindowPreview* window : previews_) {
    gfx::Size mirror_size = window->preview_view()->CalculatePreferredSize();
    float ratio = static_cast<float>(mirror_size.width()) /
                  static_cast<float>(mirror_size.height());
    max_ratio = std::max(max_ratio, ratio);
  }
  return std::min(max_ratio, kShelfTooltipPreviewMaxRatio);
}

void ShelfTooltipPreviewBubble::OnPreviewDismissed(WindowPreview* preview) {
  RemovePreview(preview);
}

void ShelfTooltipPreviewBubble::OnPreviewActivated(WindowPreview* preview) {
  // Always close the tooltip when a window has been focused.
  manager_->Close();
}

}  // namespace ash
