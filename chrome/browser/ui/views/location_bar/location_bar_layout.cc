// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"


// Description of a decoration to be added inside the location bar, either to
// the left or to the right.
struct LocationBarDecoration {
  LocationBarDecoration(int y,
                        int height,
                        bool auto_collapse,
                        double max_fraction,
                        int edge_item_padding,
                        int item_padding,
                        views::View* view);

  // The y position of the view inside its parent.
  int y;

  // The height of the view.
  int height;

  // True means that, if there is not enough available space in the location
  // bar, the view will reduce its width either to its minimal width or to zero
  // (making it invisible), whichever fits. If true, |max_fraction| must be 0.
  bool auto_collapse;

  // Used for resizeable decorations, indicates the maximum fraction of the
  // location bar that can be taken by this decoration, 0 for non-resizable
  // decorations. If non-zero, |auto_collapse| must be false.
  double max_fraction;

  // Padding to use if the decoration is the first element next to the edge.
  int edge_item_padding;

  // Padding to use if the decoration follows another decoration.
  int item_padding;

  views::View* view;

  // The width computed by the layout process.
  double computed_width;
};

LocationBarDecoration::LocationBarDecoration(int y,
                                             int height,
                                             bool auto_collapse,
                                             double max_fraction,
                                             int edge_item_padding,
                                             int item_padding,
                                             views::View* view)
    : y(y),
      height(height),
      auto_collapse(auto_collapse),
      max_fraction(max_fraction),
      edge_item_padding(edge_item_padding),
      item_padding(item_padding),
      view(view),
      computed_width(0) {
  DCHECK((max_fraction == 0.0) || (!auto_collapse && (max_fraction > 0.0)));
}


// LocationBarLayout ---------------------------------------------------------

LocationBarLayout::LocationBarLayout(Position position, int item_edit_padding)
    : position_(position),
      item_edit_padding_(item_edit_padding) {
}


LocationBarLayout::~LocationBarLayout() {
}

void LocationBarLayout::AddDecoration(int y,
                                      int height,
                                      bool auto_collapse,
                                      double max_fraction,
                                      int edge_item_padding,
                                      int item_padding,
                                      views::View* view) {
  decorations_.push_back(new LocationBarDecoration(
      y, height, auto_collapse, max_fraction, edge_item_padding, item_padding,
      view));
}

void LocationBarLayout::AddDecoration(int y,
                                      int height,
                                      views::View* view) {
  decorations_.push_back(new LocationBarDecoration(
      y, height, false, 0, LocationBarView::kItemPadding,
      LocationBarView::kItemPadding, view));
}

void LocationBarLayout::LayoutPass1(int* entry_width) {
  bool first_item = true;
  for (Decorations::iterator i(decorations_.begin()); i != decorations_.end();
       ++i) {
    // Autocollapsing decorations are ignored in this pass.
    if (!(*i)->auto_collapse) {
      *entry_width -=
          (first_item ? (*i)->edge_item_padding : (*i)->item_padding);
    }
    first_item = false;
    // Resizing decorations are ignored in this pass.
    if (!(*i)->auto_collapse && ((*i)->max_fraction == 0.0)) {
      (*i)->computed_width = (*i)->view->GetPreferredSize().width();
      *entry_width -= (*i)->computed_width;
    }
  }
  *entry_width -= item_edit_padding_;
}

void LocationBarLayout::LayoutPass2(int *entry_width) {
  for (Decorations::iterator i(decorations_.begin()); i != decorations_.end();
       ++i) {
    if ((*i)->max_fraction > 0.0) {
      int max_width = static_cast<int>(*entry_width * (*i)->max_fraction);
      (*i)->computed_width =
          std::min((*i)->view->GetPreferredSize().width(),
                   std::max((*i)->view->GetMinimumSize().width(), max_width));
      *entry_width -= (*i)->computed_width;
    }
  }
}

void LocationBarLayout::LayoutPass3(gfx::Rect* bounds, int* available_width) {
  bool first_visible = true;
  for (Decorations::iterator i(decorations_.begin()); i != decorations_.end();
       ++i) {
    // Collapse decorations if needed.
    if ((*i)->auto_collapse) {
      int padding =
          (first_visible ? (*i)->edge_item_padding : (*i)->item_padding);
      // Try preferred size, if it fails try minimum size, if it fails collapse.
      (*i)->computed_width = (*i)->view->GetPreferredSize().width();
      if ((*i)->computed_width + padding > *available_width)
        (*i)->computed_width = (*i)->view->GetMinimumSize().width();
      if ((*i)->computed_width + padding > *available_width) {
        (*i)->computed_width = 0;
        (*i)->view->SetVisible(false);
      } else {
        (*i)->view->SetVisible(true);
        (*available_width) -= (*i)->computed_width + padding;
      }
    } else {
      (*i)->view->SetVisible(true);
    }

    // Layout visible decorations.
    if (!(*i)->view->visible())
      continue;
    int padding =
        (first_visible ? (*i)->edge_item_padding : (*i)->item_padding);
    first_visible = false;
    int x = (position_ == LEFT_EDGE) ? (bounds->x() + padding) :
        (bounds->right() - padding - (*i)->computed_width);
    (*i)->view->SetBounds(x, (*i)->y, (*i)->computed_width, (*i)->height);
    bounds->set_width(bounds->width() - padding - (*i)->computed_width);
    if (position_ == LEFT_EDGE)
      bounds->set_x(bounds->x() + padding + (*i)->computed_width);
  }
  bounds->set_width(bounds->width() - item_edit_padding_);
  if (position_ == LEFT_EDGE)
    bounds->set_x(bounds->x() + item_edit_padding_);
}
