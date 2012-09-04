// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LocationIconView::LocationIconView(LocationBarView* location_bar)
    : ALLOW_THIS_IN_INITIALIZER_LIST(page_info_helper_(this, location_bar)) {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  TouchableLocationBarView::Init(this);
}

LocationIconView::~LocationIconView() {
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationIconView::OnMouseReleased(const ui::MouseEvent& event) {
  page_info_helper_.ProcessEvent(event);
}

ui::EventResult LocationIconView::OnGestureEvent(
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP) {
    page_info_helper_.ProcessEvent(event);
    return ui::ER_CONSUMED;
  }
  return ui::ER_UNHANDLED;
}

int LocationIconView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}

void LocationIconView::ShowTooltip(bool show) {
  if (show) {
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  } else {
    SetTooltipText(string16());
  }
}
