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

int HarmonyLayoutDelegate::GetMetric(Metric metric) const {
  switch (metric) {
    case Metric::DIALOG_BUTTON_MARGIN:
      return kHarmonyLayoutUnit;
    case Metric::PANEL_CONTENT_MARGIN:
      return kHarmonyLayoutUnit;
    case Metric::RELATED_BUTTON_HORIZONTAL_SPACING:
      return kHarmonyLayoutUnit / 2;
    case Metric::RELATED_CONTROL_HORIZONTAL_SPACING:
      return kHarmonyLayoutUnit;
    case Metric::RELATED_CONTROL_VERTICAL_SPACING:
      return kHarmonyLayoutUnit / 2;
    case Metric::RELATED_LABEL_HORIZONTAL_SPACING:
      return kHarmonyLayoutUnit;
    case Metric::SUBSECTION_HORIZONTAL_INDENT:
      return 0;
    case Metric::UNRELATED_CONTROL_VERTICAL_SPACING:
      return kHarmonyLayoutUnit;
    case Metric::UNRELATED_CONTROL_VERTICAL_SPACING_LARGE:
      return kHarmonyLayoutUnit;
  }
  NOTREACHED();
  return 0;
}

views::GridLayout::Alignment
HarmonyLayoutDelegate::GetControlLabelGridAlignment() const {
  return views::GridLayout::LEADING;
}

bool HarmonyLayoutDelegate::UseExtraDialogPadding() const {
  return false;
}

bool HarmonyLayoutDelegate::IsHarmonyMode() const {
  return true;
}

int HarmonyLayoutDelegate::GetDialogPreferredWidth(DialogWidth width) const {
  switch (width) {
    case DialogWidth::SMALL:
      return 320;
    case DialogWidth::MEDIUM:
      return 448;
    case DialogWidth::LARGE:
      return 512;
  }
  NOTREACHED();
  return 0;
}
