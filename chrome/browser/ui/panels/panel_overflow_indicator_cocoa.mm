// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_overflow_indicator_cocoa.h"

PanelOverflowIndicator* PanelOverflowIndicator::Create() {
  return new PanelOverflowIndicatorCocoa();
}

PanelOverflowIndicatorCocoa::PanelOverflowIndicatorCocoa() {
}

PanelOverflowIndicatorCocoa::~PanelOverflowIndicatorCocoa() {
}

int PanelOverflowIndicatorCocoa::GetHeight() const {
  return 0;
}

gfx::Rect PanelOverflowIndicatorCocoa::GetBounds() const {
  return gfx::Rect();
}

void PanelOverflowIndicatorCocoa::SetBounds(const gfx::Rect& bounds) {
}

int PanelOverflowIndicatorCocoa::GetCount() const {
  return 0;
}

void PanelOverflowIndicatorCocoa::SetCount(int count) {
}

void PanelOverflowIndicatorCocoa::DrawAttention() {
}

void PanelOverflowIndicatorCocoa::StopDrawingAttention() {
}

bool PanelOverflowIndicatorCocoa::IsDrawingAttention() const {
  return false;
}
