// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/harmony_layout_provider.h"

gfx::Insets HarmonyLayoutProvider::GetInsetsMetric(int metric) const {
  DCHECK_LT(metric, views::VIEWS_INSETS_MAX);
  switch (metric) {
    case views::INSETS_BUBBLE_CONTENTS:
    case views::INSETS_DIALOG_CONTENTS:
      return gfx::Insets(kHarmonyLayoutUnit, kHarmonyLayoutUnit);
    case views::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(kHarmonyLayoutUnit / 4);
    default:
      return ChromeLayoutProvider::GetInsetsMetric(metric);
  }
}

int HarmonyLayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, views::VIEWS_INSETS_MAX);
  switch (metric) {
    case views::DISTANCE_BUBBLE_BUTTON_TOP_MARGIN:
      return kHarmonyLayoutUnit;
    case DISTANCE_CONTROL_LIST_VERTICAL:
      return kHarmonyLayoutUnit * 3 / 4;
    case views::DISTANCE_CLOSE_BUTTON_MARGIN: {
      constexpr int kVisibleMargin = kHarmonyLayoutUnit / 2;
      // The visible margin is based on the unpadded size, so to get the actual
      // margin we need to subtract out the padding.
      return kVisibleMargin - kHarmonyLayoutUnit / 4;
    }
    case views::DISTANCE_RELATED_BUTTON_HORIZONTAL:
      return kHarmonyLayoutUnit / 2;
    case views::DISTANCE_RELATED_CONTROL_HORIZONTAL:
      return kHarmonyLayoutUnit;
    case DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL:
      return kHarmonyLayoutUnit;
    case views::DISTANCE_RELATED_CONTROL_VERTICAL:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_RELATED_CONTROL_VERTICAL_SMALL:
      return kHarmonyLayoutUnit / 2;
    case views::DISTANCE_DIALOG_BUTTON_BOTTOM_MARGIN:
      return kHarmonyLayoutUnit;
    case views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH:
    case DISTANCE_BUTTON_MINIMUM_WIDTH:
      // Minimum label size plus padding.
      return 2 * kHarmonyLayoutUnit +
             2 * GetDistanceMetric(views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
    case views::DISTANCE_BUTTON_HORIZONTAL_PADDING:
      return kHarmonyLayoutUnit;
    case views::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH:
      return kHarmonyLayoutUnit * 7;
    case DISTANCE_RELATED_LABEL_HORIZONTAL:
      return kHarmonyLayoutUnit;
    case DISTANCE_RELATED_LABEL_HORIZONTAL_LIST:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_SUBSECTION_HORIZONTAL_INDENT:
      return 0;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL:
      return kHarmonyLayoutUnit;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE:
      return kHarmonyLayoutUnit;
    case views::DISTANCE_UNRELATED_CONTROL_VERTICAL:
      return kHarmonyLayoutUnit;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE:
      return kHarmonyLayoutUnit;
  }
  NOTREACHED();
  return 0;
}

views::GridLayout::Alignment
HarmonyLayoutProvider::GetControlLabelGridAlignment() const {
  return views::GridLayout::LEADING;
}

bool HarmonyLayoutProvider::UseExtraDialogPadding() const {
  return false;
}

bool HarmonyLayoutProvider::ShouldShowWindowIcon() const {
  return false;
}

bool HarmonyLayoutProvider::IsHarmonyMode() const {
  return true;
}

int HarmonyLayoutProvider::GetSnappedDialogWidth(int min_width) const {
  for (int snap_point : {320, 448, 512}) {
    if (min_width <= snap_point)
      return snap_point;
  }

  return ((min_width + kHarmonyLayoutUnit - 1) / kHarmonyLayoutUnit) *
         kHarmonyLayoutUnit;
}

const views::TypographyProvider& HarmonyLayoutProvider::GetTypographyProvider()
    const {
  return typography_provider_;
}
