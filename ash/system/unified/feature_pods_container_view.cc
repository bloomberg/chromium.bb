// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pods_container_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"

namespace ash {

FeaturePodsContainerView::FeaturePodsContainerView() {}

FeaturePodsContainerView::~FeaturePodsContainerView() = default;

gfx::Size FeaturePodsContainerView::CalculatePreferredSize() const {
  const int collapsed_height = 2 * kUnifiedFeaturePodCollapsedVerticalPadding +
                               kUnifiedFeaturePodCollapsedSize.height();

  int visible_count = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (static_cast<const FeaturePodButton*>(child_at(i))->visible_preferred())
      ++visible_count;
  }

  // floor(visible_count / kUnifiedFeaturePodItemsInRow)
  int number_of_lines = (visible_count + kUnifiedFeaturePodItemsInRow - 1) /
                        kUnifiedFeaturePodItemsInRow;
  const int expanded_height =
      kUnifiedFeaturePodVerticalPadding +
      (kUnifiedFeaturePodVerticalPadding + kUnifiedFeaturePodSize.height()) *
          number_of_lines;

  return gfx::Size(
      kTrayMenuWidth,
      static_cast<int>(collapsed_height * (1.0 - expanded_amount_) +
                       expanded_height * expanded_amount_));
}

void FeaturePodsContainerView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  if (expanded_amount_ == expanded_amount)
    return;
  expanded_amount_ = expanded_amount;

  PreferredSizeChanged();

  if (expanded_amount == 0.0 || expanded_amount == 1.0) {
    expanded_ = expanded_amount == 1.0;
    for (int i = 0; i < child_count(); ++i) {
      auto* child = static_cast<FeaturePodButton*>(child_at(i));
      child->SetExpanded(expanded_);
    }
    UpdateChildVisibility();
    // We have to call Layout() explicitly here.
    Layout();
  }
}

void FeaturePodsContainerView::ChildVisibilityChanged(View* child) {
  // ChildVisibilityChanged can change child visibility in
  // UpdateChildVisibility(), so we have to prevent this.
  if (changing_visibility_)
    return;

  UpdateChildVisibility();
  Layout();
  SchedulePaint();
}

void FeaturePodsContainerView::Layout() {
  if (expanded_)
    LayoutExpanded();
  else
    LayoutCollapsed();
}

void FeaturePodsContainerView::LayoutExpanded() {
  DCHECK(expanded_);
  int x = kUnifiedFeaturePodHorizontalSidePadding;
  int y = kUnifiedFeaturePodVerticalPadding;
  int visible_count = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    ++visible_count;

    child->SetBounds(x, y, kUnifiedFeaturePodSize.width(),
                     kUnifiedFeaturePodSize.height());
    child->Layout();

    x += kUnifiedFeaturePodSize.width() +
         kUnifiedFeaturePodHorizontalMiddlePadding;
    // Go to the next line if the number of items exceeds
    // kUnifiedFeaturePodItemsInRow.
    if (visible_count % kUnifiedFeaturePodItemsInRow == 0) {
      // Check if the total width fits into the menu width.
      DCHECK(x - kUnifiedFeaturePodHorizontalMiddlePadding +
                 kUnifiedFeaturePodHorizontalSidePadding ==
             kTrayMenuWidth);
      x = kUnifiedFeaturePodHorizontalSidePadding;
      y += kUnifiedFeaturePodSize.height() + kUnifiedFeaturePodVerticalPadding;
    }
  }
}

void FeaturePodsContainerView::LayoutCollapsed() {
  DCHECK(!expanded_);

  int visible_count = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->visible())
      ++visible_count;
  }

  DCHECK(visible_count > 0 &&
         visible_count <= kUnifiedFeaturePodMaxItemsInCollapsed);

  int contents_width =
      visible_count * kUnifiedFeaturePodCollapsedSize.width() +
      (visible_count - 1) * kUnifiedFeaturePodCollapsedHorizontalPadding;

  int side_padding = (width() - contents_width) / 2;
  DCHECK(side_padding > 0);

  int x = side_padding;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;

    child->SetBounds(x, kUnifiedFeaturePodCollapsedVerticalPadding,
                     kUnifiedFeaturePodCollapsedSize.width(),
                     kUnifiedFeaturePodCollapsedSize.height());

    x += kUnifiedFeaturePodCollapsedSize.width() +
         kUnifiedFeaturePodCollapsedHorizontalPadding;
  }

  DCHECK(x - kUnifiedFeaturePodCollapsedHorizontalPadding + side_padding ==
         kTrayMenuWidth);
}

void FeaturePodsContainerView::UpdateChildVisibility() {
  DCHECK(!changing_visibility_);
  changing_visibility_ = true;

  int visible_count = 0;
  for (int i = 0; i < child_count(); ++i) {
    auto* child = static_cast<FeaturePodButton*>(child_at(i));
    child->SetVisibleByContainer(
        child->visible_preferred() &&
        (expanded_ || visible_count < kUnifiedFeaturePodMaxItemsInCollapsed));
    if (child->visible())
      ++visible_count;
  }

  changing_visibility_ = false;
}

}  // namespace ash
