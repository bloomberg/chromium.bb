// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "ui/views/controls/image_view.h"

namespace views {
class KeyEvent;
class MouseEvent;
}

// View for the zoom icon in the Omnibox.
class ZoomView : public views::ImageView {
 public:
  explicit ZoomView(ToolbarModel* toolbar_model);
  virtual ~ZoomView();

  void SetZoomIconState(ZoomController::ZoomIconState zoom_icon_state);
  void SetZoomIconTooltipPercent(int zoom_percent);

  // Updates the image and its tooltip appropriately, hiding or showing the icon
  // as needed.
  void Update();

 private:
  // views::ImageView:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;

  // Toolbar model used to test whether location bar input is in progress.
  ToolbarModel* toolbar_model_;

  // The current icon state.
  ZoomController::ZoomIconState zoom_icon_state_;

  // The current zoom percentage.
  int zoom_percent_;

  DISALLOW_COPY_AND_ASSIGN(ZoomView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
