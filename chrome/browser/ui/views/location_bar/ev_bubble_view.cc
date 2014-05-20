// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"
#include "grit/theme_resources.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"


namespace {
const int kBackgroundImages[] = IMAGE_GRID(IDR_OMNIBOX_EV_BUBBLE);
}


EVBubbleView::EVBubbleView(const gfx::FontList& font_list,
                           SkColor text_color,
                           SkColor parent_background_color,
                           LocationBarView* location_bar)
    : IconLabelBubbleView(kBackgroundImages, NULL, IDR_OMNIBOX_HTTPS_VALID,
                          font_list, text_color, parent_background_color, true),
      page_info_helper_(this, location_bar) {
}

EVBubbleView::~EVBubbleView() {
}

gfx::Size EVBubbleView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
}

bool EVBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void EVBubbleView::OnMouseReleased(const ui::MouseEvent& event) {
  page_info_helper_.ProcessEvent(event);
}

void EVBubbleView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    page_info_helper_.ProcessEvent(*event);
    event->SetHandled();
  }
}

gfx::Size EVBubbleView::GetMinimumSizeForLabelText(
    const base::string16& text) const {
  views::Label label(text, font_list());
  return GetMinimumSizeForPreferredSize(
      GetSizeForLabelWidth(label.GetPreferredSize().width()));
}

gfx::Size EVBubbleView::GetMinimumSizeForPreferredSize(gfx::Size size) const {
  const int kMinCharacters = 10;
  size.SetToMin(
      GetSizeForLabelWidth(font_list().GetExpectedTextWidth(kMinCharacters)));
  return size;
}
