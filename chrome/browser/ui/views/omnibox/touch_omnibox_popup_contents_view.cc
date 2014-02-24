// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/touch_omnibox_popup_contents_view.h"

#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"

// TouchOmniboxResultView ------------------------------------------------

TouchOmniboxResultView::TouchOmniboxResultView(
    OmniboxPopupContentsView* model,
    int model_index,
    LocationBarView* location_bar_view,
    const gfx::FontList& font_list)
    : OmniboxResultView(model, model_index, location_bar_view, font_list) {
  set_edge_item_padding(8);
  set_item_padding(8);
  set_minimum_text_vertical_padding(10);
}

TouchOmniboxResultView::~TouchOmniboxResultView() {
}

void TouchOmniboxResultView::PaintMatch(gfx::Canvas* canvas, int x) {
  const AutocompleteMatch& match = display_match();
  int y = text_bounds().y();

  if (!match.description.empty()) {
    // We use our base class's GetTextHeight below because we need the height
    // of a single line of text.
    scoped_ptr<gfx::RenderText> render_text(
        CreateRenderText(match.description));
    ApplyClassifications(render_text.get(), match.description_class, true);
    DrawRenderText(canvas, render_text.get(), false, x, y);
    y += OmniboxResultView::GetTextHeight();
  } else {
    // When we have only one line of content (no description), we center the
    // single line vertically on our two-lines-tall results box.
    y += OmniboxResultView::GetTextHeight() / 2;
  }

  DrawRenderText(canvas, RenderMatchContents(), true, x, y);
}

int TouchOmniboxResultView::GetTextHeight() const {
  return OmniboxResultView::GetTextHeight() * 2;
}

// TouchOmniboxPopupContentsView -----------------------------------------

TouchOmniboxPopupContentsView::TouchOmniboxPopupContentsView(
    const gfx::FontList& font_list,
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    LocationBarView* location_bar_view)
    : OmniboxPopupContentsView(font_list, omnibox_view, edit_model,
                               location_bar_view) {
}

TouchOmniboxPopupContentsView::~TouchOmniboxPopupContentsView() {
}

void TouchOmniboxPopupContentsView::UpdatePopupAppearance() {
  OmniboxPopupContentsView::UpdatePopupAppearance();
  Layout();
}

void TouchOmniboxPopupContentsView::PaintResultViews(gfx::Canvas* canvas) {
  OmniboxPopupContentsView::PaintResultViews(canvas);

  // Draw divider lines.
  std::vector<View*> visible_children(GetVisibleChildren());
  if (visible_children.size() < 2)
    return;
  gfx::Rect bounds(GetContentsBounds());

  // Draw a line at the bottom of each child except the last.  The
  // color of the line is determined to blend appropriately with the
  // most dominant of the two surrounding cells, in precedence order,
  // i.e. selected > hovered > normal.
  for (std::vector<View*>::const_iterator i(visible_children.begin());
       i + 1 != visible_children.end(); ++i) {
    TouchOmniboxResultView* child = static_cast<TouchOmniboxResultView*>(*i);
    TouchOmniboxResultView* next_child =
        static_cast<TouchOmniboxResultView*>(*(i + 1));
    SkColor divider_color = child->GetColor(
        std::max(child->GetState(), next_child->GetState()),
        OmniboxResultView::DIVIDER);
    int line_y = child->y() + child->height() - 1;
    canvas->DrawLine(gfx::Point(bounds.x(), line_y),
                     gfx::Point(bounds.right(), line_y), divider_color);
  }
}

OmniboxResultView* TouchOmniboxPopupContentsView::CreateResultView(
    int model_index,
    const gfx::FontList& font_list) {
  return new TouchOmniboxResultView(this, model_index, location_bar_view(),
                                    font_list);
}

std::vector<views::View*> TouchOmniboxPopupContentsView::GetVisibleChildren() {
  std::vector<View*> visible_children;
  for (int i = 0; i < child_count(); ++i) {
    View* v = child_at(i);
    if (child_at(i)->visible())
      visible_children.push_back(v);
  }
  return visible_children;
}
