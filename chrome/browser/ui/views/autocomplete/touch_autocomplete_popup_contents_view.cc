// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/touch_autocomplete_popup_contents_view.h"

#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"


// TouchAutocompleteResultView ------------------------------------------------

TouchAutocompleteResultView::TouchAutocompleteResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const gfx::Font& font,
    const gfx::Font& bold_font)
    : AutocompleteResultView(model, model_index, font, bold_font) {
}

TouchAutocompleteResultView::~TouchAutocompleteResultView() {
}

void TouchAutocompleteResultView::PaintMatch(gfx::Canvas* canvas,
                                             const AutocompleteMatch& match,
                                             int x) {
  DrawString(canvas, match.contents, match.contents_class, false, x,
             text_bounds().y());

  if (!match.description.empty()) {
    // We use our base class's GetTextHeight below because we need the height
    // of a single line of text.
    DrawString(canvas, match.description, match.description_class, true, x,
        text_bounds().y() + AutocompleteResultView::GetTextHeight());
  }
}

int TouchAutocompleteResultView::GetTextHeight() const {
  // In the touch version of the autocomplete popup, the text is displayed in
  // two lines: First line is the title of the suggestion and second is the
  // description. Hence, the total text height is twice the height of one line.
  return AutocompleteResultView::GetTextHeight() * 2;
}


// TouchAutocompletePopupContentsView -----------------------------------------

TouchAutocompletePopupContentsView::TouchAutocompletePopupContentsView(
    const gfx::Font& font,
    OmniboxView* omnibox_view,
    AutocompleteEditModel* edit_model,
    views::View* location_bar)
    : AutocompletePopupContentsView(font, omnibox_view, edit_model,
                                    location_bar) {
}

TouchAutocompletePopupContentsView::~TouchAutocompletePopupContentsView() {
}

void TouchAutocompletePopupContentsView::UpdatePopupAppearance() {
  AutocompletePopupContentsView::UpdatePopupAppearance();
  Layout();
}

void TouchAutocompletePopupContentsView::LayoutChildren() {
  std::vector<View*> visible_children(GetVisibleChildren());
  gfx::Rect bounds(GetContentsBounds());
  double child_width =
      static_cast<double>(bounds.width()) / visible_children.size();
  int x = bounds.x();
  for (size_t i = 0; i < visible_children.size(); ++i) {
    int next_x = bounds.x() + static_cast<int>(((i + 1) * child_width) + 0.5);
    visible_children[i]->SetBounds(x, bounds.y(), next_x - x, bounds.height());
    x = next_x;
  }
}

void TouchAutocompletePopupContentsView::PaintResultViews(
    gfx::CanvasSkia* canvas) {
  AutocompletePopupContentsView::PaintResultViews(canvas);

  // Draw divider lines.
  std::vector<View*> visible_children(GetVisibleChildren());
  if (visible_children.size() < 2)
    return;
  SkColor color = AutocompleteResultView::GetColor(
      AutocompleteResultView::NORMAL, AutocompleteResultView::DIMMED_TEXT);
  gfx::Rect bounds(GetContentsBounds());
  for (std::vector<View*>::const_iterator i(visible_children.begin() + 1);
       i != visible_children.end(); ++i) {
    canvas->DrawLineInt(color, (*i)->x(), bounds.y(), (*i)->x(),
                        bounds.bottom());
  }
}

int TouchAutocompletePopupContentsView::CalculatePopupHeight() {
  DCHECK_GE(static_cast<size_t>(child_count()), model_->result().size());
  int popup_height = 0;
  for (size_t i = 0; i < model_->result().size(); ++i) {
    popup_height = std::max(popup_height,
                            child_at(i)->GetPreferredSize().height());
  }
  popup_height = std::max(popup_height, opt_in_view_ ?
      opt_in_view_->GetPreferredSize().height() : 0);
  return popup_height;
}

AutocompleteResultView* TouchAutocompletePopupContentsView::CreateResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const gfx::Font& font,
    const gfx::Font& bold_font) {
  return new TouchAutocompleteResultView(model, model_index, font, bold_font);
}

std::vector<views::View*>
    TouchAutocompletePopupContentsView::GetVisibleChildren() {
  std::vector<View*> visible_children;
  for (int i = 0; i < child_count(); ++i) {
    View* v = child_at(i);
    if (child_at(i)->IsVisible())
      visible_children.push_back(v);
  }
  return visible_children;
}
