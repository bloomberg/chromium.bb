// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

namespace {

enum DecorationType {
  // Decoration is always visible.
  NORMAL = 0,
  // If there is not enough available space in the location bar, the decoration
  // will reduce its width either to its minimal width or to zero (making it
  // invisible), whichever fits.  |LocationBarDecoration::max_fraction| must be
  // 0.
  AUTO_COLLAPSE,
  // Decoration is a separator, only visible if it's not leading, or not
  // trailing, or not next to another separator.
  SEPARATOR,
};

}  // namespace


// Description of a decoration to be added inside the location bar, either to
// the left or to the right.
struct LocationBarDecoration {
  LocationBarDecoration(DecorationType type,
                        int y,
                        int height,
                        double max_fraction,
                        int edge_item_padding,
                        int item_padding,
                        int builtin_padding,
                        views::View* view);

  // The type of decoration.
  DecorationType type;

  // The y position of the view inside its parent.
  int y;

  // If 0, will use the preferred height of the view.
  int height;

  // Used for resizeable decorations, indicates the maximum fraction of the
  // location bar that can be taken by this decoration, 0 for non-resizable
  // decorations. If non-zero, |type| must not be AUTO_COLLAPSE.
  double max_fraction;

  // Padding to use if the decoration is the first element next to the edge.
  int edge_item_padding;

  // Padding to use if the decoration follows another decoration.
  int item_padding;

  // Padding built into the decoration and that should be removed, on
  // both sides, during layout.
  int builtin_padding;

  views::View* view;

  // The width computed by the layout process.
  double computed_width;
};

LocationBarDecoration::LocationBarDecoration(DecorationType type,
                                             int y,
                                             int height,
                                             double max_fraction,
                                             int edge_item_padding,
                                             int item_padding,
                                             int builtin_padding,
                                             views::View* view)
    : type(type),
      y(y),
      height(height),
      max_fraction(max_fraction),
      edge_item_padding(edge_item_padding),
      item_padding(item_padding),
      builtin_padding(builtin_padding),
      view(view),
      computed_width(0) {
  if (type == NORMAL) {
    DCHECK_GE(max_fraction, 0.0);
  } else {
    DCHECK_EQ(0.0, max_fraction);
  }
}


// LocationBarLayout ---------------------------------------------------------

LocationBarLayout::LocationBarLayout(Position position,
                                     int item_edit_padding,
                                     int edge_edit_padding)
    : position_(position),
      item_edit_padding_(item_edit_padding),
      edge_edit_padding_(edge_edit_padding) {}


LocationBarLayout::~LocationBarLayout() {
}

void LocationBarLayout::AddDecoration(int y,
                                      int height,
                                      bool auto_collapse,
                                      double max_fraction,
                                      int edge_item_padding,
                                      int item_padding,
                                      int builtin_padding,
                                      views::View* view) {
  decorations_.push_back(new LocationBarDecoration(
      auto_collapse ? AUTO_COLLAPSE : NORMAL, y, height, max_fraction,
      edge_item_padding, item_padding, builtin_padding, view));
}

void LocationBarLayout::AddDecoration(int height,
                                      int builtin_padding,
                                      views::View* view) {
  decorations_.push_back(new LocationBarDecoration(
      NORMAL, LocationBarView::kVerticalEdgeThickness, height, 0,
      LocationBarView::GetEdgeItemPadding(), LocationBarView::GetItemPadding(),
      builtin_padding, view));
}

void LocationBarLayout::AddSeparator(int y,
                                     int height,
                                     int padding_from_previous_item,
                                     views::View* separator) {
  // Edge item padding won't apply since a separator won't be by the edge, so
  // use 0 for more accurate evaluation of |entry_width| in LayoutPass1().
  decorations_.push_back(new LocationBarDecoration(
      SEPARATOR, y, height, 0, 0, padding_from_previous_item, 0, separator));
}

void LocationBarLayout::LayoutPass1(int* entry_width) {
  bool first_item = true;
  bool at_least_one_visible = false;
  for (Decorations::iterator it(decorations_.begin()); it != decorations_.end();
       ++it) {
    // Autocollapsing decorations are ignored in this pass.
    if ((*it)->type != AUTO_COLLAPSE) {
      at_least_one_visible = true;
      *entry_width -= -2 * (*it)->builtin_padding +
          (first_item ? (*it)->edge_item_padding : (*it)->item_padding);
    }
    first_item = false;
    // Resizing decorations are ignored in this pass.
    if (((*it)->type != AUTO_COLLAPSE) && ((*it)->max_fraction == 0.0)) {
      (*it)->computed_width = (*it)->view->GetPreferredSize().width();
      *entry_width -= (*it)->computed_width;
    }
  }
  *entry_width -= at_least_one_visible ? item_edit_padding_ :
      edge_edit_padding_;
}

void LocationBarLayout::LayoutPass2(int *entry_width) {
  for (Decorations::iterator it(decorations_.begin()); it != decorations_.end();
       ++it) {
    if ((*it)->max_fraction > 0.0) {
      int max_width = static_cast<int>(*entry_width * (*it)->max_fraction);
      (*it)->computed_width = std::min((*it)->view->GetPreferredSize().width(),
          std::max((*it)->view->GetMinimumSize().width(), max_width));
      *entry_width -= (*it)->computed_width;
    }
  }
}

void LocationBarLayout::LayoutPass3(gfx::Rect* bounds, int* available_width) {
  SetVisibilityForDecorations(available_width);
  HideUnneededSeparators(available_width);
  SetBoundsForDecorations(bounds);
}

void LocationBarLayout::SetVisibilityForDecorations(int* available_width) {
  bool first_visible = true;
  for (Decorations::iterator it(decorations_.begin()); it != decorations_.end();
       ++it) {
    // Collapse decorations if needed.
    if ((*it)->type == AUTO_COLLAPSE) {
      int padding = -2 * (*it)->builtin_padding +
          (first_visible ? (*it)->edge_item_padding : (*it)->item_padding);
      // Try preferred size, if it fails try minimum size, if it fails
      // collapse.
      (*it)->computed_width = (*it)->view->GetPreferredSize().width();
      if ((*it)->computed_width + padding > *available_width)
        (*it)->computed_width = (*it)->view->GetMinimumSize().width();
      if ((*it)->computed_width + padding > *available_width) {
        (*it)->computed_width = 0;
        (*it)->view->SetVisible(false);
      } else {
        (*it)->view->SetVisible(true);
        (*available_width) -= (*it)->computed_width + padding;
      }
    } else {
      (*it)->view->SetVisible(true);
    }

    if ((*it)->view->visible())
      first_visible = false;
  }
}

void LocationBarLayout::HideUnneededSeparators(int* available_width) {
  // Initialize |trailing_separator| to first decoration so that any leading
  // separator will be hidden.
  Decorations::iterator trailing_separator = decorations_.begin();
  for (Decorations::iterator it(decorations_.begin()); it != decorations_.end();
       ++it) {
    if ((*it)->type == SEPARATOR) {
      if (trailing_separator != decorations_.end()) {
        (*it)->view->SetVisible(false);
        // Add back what was subtracted when setting this separator visible in
        // LayoutPass1().
        (*available_width) += (*it)->item_padding + (*it)->computed_width;
      } else {
        trailing_separator = it;
      }
    } else if ((*it)->view->visible()) {
      trailing_separator = decorations_.end();
    }
  }
  // If there's a trailing separator, hide it.
  if (trailing_separator != decorations_.end()) {
    (*trailing_separator)->view->SetVisible(false);
    // Add back what was subtracted when setting this separator visible in
    // LayoutPass1().
    (*available_width) += (*trailing_separator)->item_padding +
                          (*trailing_separator)->computed_width;
  }
}

void LocationBarLayout::SetBoundsForDecorations(gfx::Rect* bounds) {
  bool first_visible = true;
  for (Decorations::iterator it(decorations_.begin()); it != decorations_.end();
       ++it) {
    LocationBarDecoration* curr = *it;
    if (!curr->view->visible())
      continue;
    int padding = -curr->builtin_padding +
        (first_visible ? curr->edge_item_padding : curr->item_padding);
    first_visible = false;
    int x = (position_ == LEFT_EDGE) ? (bounds->x() + padding) :
        (bounds->right() - padding - curr->computed_width);
    int height = curr->height == 0 ?
        curr->view->GetPreferredSize().height() : curr->height;
    curr->view->SetBounds(x, curr->y, curr->computed_width, height);
    bounds->set_width(bounds->width() - padding - curr->computed_width +
                      curr->builtin_padding);
    if (position_ == LEFT_EDGE) {
      bounds->set_x(
          bounds->x() + padding + curr->computed_width - curr->builtin_padding);
    }
  }
  int final_padding = first_visible ? edge_edit_padding_ : item_edit_padding_;
  bounds->set_width(bounds->width() - final_padding);
  if (position_ == LEFT_EDGE)
    bounds->set_x(bounds->x() + final_padding);
}
