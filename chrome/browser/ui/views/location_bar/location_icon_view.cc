// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LocationIconView::LocationIconView(LocationBarView* location_bar)
    : page_info_helper_(this, location_bar) {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  LocationBarView::InitTouchableLocationBarChildView(this);
}

LocationIconView::~LocationIconView() {
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationIconView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!chrome::ShouldDisplayOriginChip())
    page_info_helper_.ProcessEvent(event);
}

void LocationIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (!chrome::ShouldDisplayOriginChip() &&
      (event->type() == ui::ET_GESTURE_TAP)) {
    page_info_helper_.ProcessEvent(*event);
    event->SetHandled();
  }
}

void LocationIconView::ShowTooltip(bool show) {
  SetTooltipText(show ?
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON) : base::string16());
}
