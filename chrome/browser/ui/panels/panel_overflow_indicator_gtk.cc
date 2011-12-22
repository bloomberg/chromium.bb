// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_overflow_indicator_gtk.h"

PanelOverflowIndicator* PanelOverflowIndicator::Create() {
  return new PanelOverflowIndicatorGtk();
}

PanelOverflowIndicatorGtk::PanelOverflowIndicatorGtk() {
}

PanelOverflowIndicatorGtk::~PanelOverflowIndicatorGtk() {
}

int PanelOverflowIndicatorGtk::GetHeight() const {
  return 0;
}

gfx::Rect PanelOverflowIndicatorGtk::GetBounds() const {
  return gfx::Rect();
}

void PanelOverflowIndicatorGtk::SetBounds(const gfx::Rect& bounds) {
}

int PanelOverflowIndicatorGtk::GetCount() const {
  return 0;
}

void PanelOverflowIndicatorGtk::SetCount(int count) {
}

void PanelOverflowIndicatorGtk::DrawAttention() {
}

void PanelOverflowIndicatorGtk::StopDrawingAttention() {
}

bool PanelOverflowIndicatorGtk::IsDrawingAttention() const {
  return false;
}
