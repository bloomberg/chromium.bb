// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/dom_browser_view_layout.h"

#include <algorithm>

#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "views/window/hit_test.h"

namespace chromeos {

// DOMBrowserViewLayout public: ------------------------------------------------

DOMBrowserViewLayout::DOMBrowserViewLayout() : ::BrowserViewLayout() {}

DOMBrowserViewLayout::~DOMBrowserViewLayout() {}

// DOMBrowserViewLayout, ::DOMBrowserViewLayout overrides: ---------------------

void DOMBrowserViewLayout::Installed(views::View* host) {
  status_area_ = NULL;
  ::BrowserViewLayout::Installed(host);
}

void DOMBrowserViewLayout::ViewAdded(views::View* host,
                                     views::View* view) {
  ::BrowserViewLayout::ViewAdded(host, view);
  switch (view->GetID()) {
    case VIEW_ID_STATUS_AREA:
      status_area_ = static_cast<chromeos::StatusAreaView*>(view);
      break;
  }
}

int DOMBrowserViewLayout::LayoutTabStrip() {
  status_area_->SetVisible(true);
  gfx::Size status_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(vertical_layout_rect_.width() - status_size.width(),
                          0,
                          vertical_layout_rect_.width(),
                          status_size.height());

  return status_size.height();
}

int DOMBrowserViewLayout::LayoutToolbar(int top) {
  return top;
}

int DOMBrowserViewLayout::LayoutBookmarkAndInfoBars(int top) {
  return top;
}

bool DOMBrowserViewLayout::IsPositionInWindowCaption(const gfx::Point& point) {
  return false;
}

int DOMBrowserViewLayout::NonClientHitTest(const gfx::Point& point) {
  views::View* parent = browser_view_->parent();
  gfx::Point point_in_browser_view_coords(point);
  views::View::ConvertPointToView(
      parent, browser_view_, &point_in_browser_view_coords);
  gfx::Rect bv_bounds = browser_view_->bounds();
  if (bv_bounds.Contains(point))
    return HTCLIENT;
  // If the point is somewhere else, delegate to the default implementation.
  return browser_view_->views::ClientView::NonClientHitTest(point);
}

// DOMBrowserViewLayout private: -----------------------------------------------

bool DOMBrowserViewLayout::IsPointInViewsInTitleArea(const gfx::Point& point)
    const {
  gfx::Point point_in_status_area_coords(point);
  views::View::ConvertPointToView(browser_view_, status_area_,
                                  &point_in_status_area_coords);
  return status_area_->HitTest(point_in_status_area_coords);
}

int DOMBrowserViewLayout::LayoutTitlebarComponents(const gfx::Rect& bounds) {
  status_area_->SetVisible(true);
  gfx::Size status_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(bounds.right() - status_size.width(),
                          bounds.y(),
                          status_size.width(),
                          status_size.height());
  return status_size.height();
}

const DOMBrowserView* DOMBrowserViewLayout::GetDOMBrowserView() {
  return static_cast<DOMBrowserView*>(browser_view_);
}

}  // namespace chromeos
