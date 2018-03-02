// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pods_container_view.h"

#include "ash/system/tray/tray_constants.h"

namespace ash {

FeaturePodsContainerView::FeaturePodsContainerView() {}

FeaturePodsContainerView::~FeaturePodsContainerView() = default;

gfx::Size FeaturePodsContainerView::CalculatePreferredSize() const {
  int visible_count = 0;
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->visible())
      ++visible_count;
  }

  // floor(visible_count / kUnifiedFeaturePodItemsInRow)
  int number_of_lines = (visible_count + kUnifiedFeaturePodItemsInRow - 1) /
                        kUnifiedFeaturePodItemsInRow;
  return gfx::Size(0, kUnifiedFeaturePodVerticalPadding +
                          (kUnifiedFeaturePodVerticalPadding +
                           kUnifiedFeaturePodSize.height()) *
                              number_of_lines);
}

void FeaturePodsContainerView::ChildVisibilityChanged(View* child) {
  Layout();
  SchedulePaint();
}

void FeaturePodsContainerView::Layout() {
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

}  // namespace ash
