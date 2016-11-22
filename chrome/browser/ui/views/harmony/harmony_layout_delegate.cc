// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/harmony_layout_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"

static base::LazyInstance<HarmonyLayoutDelegate> harmony_layout_delegate_ =
    LAZY_INSTANCE_INITIALIZER;

// static
HarmonyLayoutDelegate* HarmonyLayoutDelegate::Get() {
  return harmony_layout_delegate_.Pointer();
}

int HarmonyLayoutDelegate::GetLayoutDistance(LayoutDistanceType type) const {
  switch (type) {
    case LayoutDistanceType::PANEL_VERT_MARGIN:
      return kHarmonyLayoutUnit;
    case LayoutDistanceType::RELATED_BUTTON_HORIZONTAL_SPACING:
      return kHarmonyLayoutUnit / 2;
    case LayoutDistanceType::RELATED_CONTROL_HORIZONTAL_SPACING:
      return kHarmonyLayoutUnit;
    case LayoutDistanceType::RELATED_CONTROL_VERTICAL_SPACING:
      return kHarmonyLayoutUnit / 2;
    case LayoutDistanceType::UNRELATED_CONTROL_VERTICAL_SPACING:
      return kHarmonyLayoutUnit;
    case LayoutDistanceType::UNRELATED_CONTROL_LARGE_VERTICAL_SPACING:
      return kHarmonyLayoutUnit;
    case LayoutDistanceType::BUTTON_VEDGE_MARGIN_NEW:
      return kHarmonyLayoutUnit;
    case LayoutDistanceType::BUTTON_HEDGE_MARGIN_NEW:
      return kHarmonyLayoutUnit;
  }
  NOTREACHED();
  return 0;
}

bool HarmonyLayoutDelegate::UseExtraDialogPadding() const {
  return false;
}

bool HarmonyLayoutDelegate::IsHarmonyMode() const {
  return true;
}

int HarmonyLayoutDelegate::GetDialogPreferredWidth(DialogWidthType type) const {
  switch (type) {
    case DialogWidthType::SMALL:
      return 320;
    case DialogWidthType::MEDIUM:
      return 448;
    case DialogWidthType::LARGE:
      return 512;
  }
  NOTREACHED();
  return 0;
}
