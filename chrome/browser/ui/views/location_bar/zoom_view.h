// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

class ZoomController;

// View for the zoom icon in the Omnibox.
class ZoomView : public views::ImageView {
 public:
  // Clicking on the ZoomView shows a ZoomBubbleView, which requires the current
  // WebContents. Because the current WebContents changes as the user switches
  // tabs, it cannot be provided in the constructor. Instead, a
  // LocationBarView::Delegate is passed here so that it can be queried for the
  // current WebContents as needed.
  explicit ZoomView(LocationBarView::Delegate* location_bar_delegate);
  virtual ~ZoomView();

  // Updates the image and its tooltip appropriately, hiding or showing the icon
  // as needed.
  void Update(ZoomController* zoom_controller);

 private:
  // views::ImageView:
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Helper method to show and focus the zoom bubble associated with this
  // widget.
  void ActivateBubble();

  // The delegate used to get the currently visible WebContents.
  LocationBarView::Delegate* location_bar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ZoomView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
