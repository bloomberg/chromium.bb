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
                        int builtin_padding,
                        views::View* view);

  // The y position of the view inside its parent.
  int y;

  // If 0, will use the preferred height of the view.
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

  // Padding built into the decoration and that should be removed, on
  // both sides, during layout.
  int builtin_padding;

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
                                             int builtin_padding,
                                             views::View* view)
    : y(y),
      height(height),
      auto_collapse(auto_collapse),
      max_fraction(max_fraction),
      edge_item_padding(edge_item_padding),
      item_padding(item_padding),
      builtin_padding(builtin_padding),
      view(view),
      computed_width(0) {
  DCHECK(!auto_collapse || max_fraction == 0.0);
  DCHECK(max_fraction >= 0.0);
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
  decorations_.push_back(new LocationBarDecoration(y, height, auto_collapse,
      max_fraction, edge_item_padding, item_padding, builtin_padding, view));
}

void LocationBarLayout::AddDecoration(int height,
                                      int builtin_padding,
                                      views::View* view) {
  decorations_.push_back(new LocationBarDecoration(
      LocationBarView::kVerticalEdgeThickness, height, false, 0,
      LocationBarView::GetEdgeItemPadding(), LocationBarView::GetItemPadding(),
      builtin_padding, view));
}

void LocationBarLayout::LayoutPass1(int* entry_width) {

  bool first_item = true;
  bool at_least_one_visible = false;
  for (ScopedVector<LocationBarDecoration>::iterator it(decorations_.begin());
       it != decorations_.end(); ++it) {
    // Autocollapsing decorations are ignored in this pass.
    if (!(*it)->auto_collapse) {
      at_least_one_visible = true;
      *entry_width -= -2 * (*it)->builtin_padding +
          (first_item ? (*it)->edge_item_padding : (*it)->item_padding);
    }
    first_item = false;
    // Resizing decorations are ignored in this pass.
    if (!(*it)->auto_collapse && (*it)->max_fraction == 0.0) {
      (*it)->computed_width = (*it)->view->GetPreferredSize().width();
      *entry_width -= (*it)->computed_width;
    }
  }
  *entry_width -= at_least_one_visible ? item_edit_padding_ :
      edge_edit_padding_;
}

void LocationBarLayout::LayoutPass2(int *entry_width) {
  for (ScopedVector<LocationBarDecoration>::iterator it(decorations_.begin());
       it != decorations_.end(); ++it) {
    if ((*it)->max_fraction > 0.0) {
      int max_width = static_cast<int>(*entry_width * (*it)->max_fraction);
      (*it)->computed_width = std::min((*it)->view->GetPreferredSize().width(),
          std::max((*it)->view->GetMinimumSize().width(), max_width));
      *entry_width -= (*it)->computed_width;
    }
  }
}

void LocationBarLayout::LayoutPass3(gfx::Rect* bounds, int* available_width) {
  bool first_visible = true;
  for (ScopedVector<LocationBarDecoration>::iterator it(decorations_.begin());
       it != decorations_.end(); ++it) {
    // Collapse decorations if needed.
    if ((*it)->auto_collapse) {
      int padding = -2 * (*it)->builtin_padding +
          (first_visible ? (*it)->edge_item_padding : (*it)->item_padding);
      // Try preferred size, if it fails try minimum size, if it fails collapse.
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
    // Layout visible decorations.
    if ((*it)->view->visible()) {
      int padding = -(*it)->builtin_padding +
          (first_visible ? (*it)->edge_item_padding : (*it)->item_padding);
      first_visible = false;
      int x;
      if (position_ == LEFT_EDGE)
        x = bounds->x() + padding;
      else
        x = bounds->x() + bounds->width() - padding - (*it)->computed_width;
      int height = (*it)->height == 0 ?
          (*it)->view->GetPreferredSize().height() : (*it)->height;
      (*it)->view->SetBounds(x, (*it)->y, (*it)->computed_width, height);
      bounds->set_width(bounds->width() - padding - (*it)->computed_width +
                        (*it)->builtin_padding);
      if (position_ == LEFT_EDGE) {
        bounds->set_x(bounds->x() + padding + (*it)->computed_width -
                      (*it)->builtin_padding);
      }
    }
  }
  int final_padding = first_visible ? edge_edit_padding_ : item_edit_padding_;
  bounds->set_width(bounds->width() - final_padding);
  if (position_ == LEFT_EDGE)
    bounds->set_x(bounds->x() + final_padding);
}
