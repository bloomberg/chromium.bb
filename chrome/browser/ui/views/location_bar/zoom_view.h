// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

namespace zoom {
class ZoomController;
}

// View for the zoom icon in the Omnibox.
class ZoomView : public BubbleIconView {
 public:
  // Clicking on the ZoomView shows a ZoomBubbleView, which requires the current
  // WebContents. Because the current WebContents changes as the user switches
  // tabs, a LocationBarView::Delegate is supplied to queried for the current
  // WebContents when needed.
  explicit ZoomView(LocationBarView::Delegate* location_bar_delegate);
  ~ZoomView() override;

  // Updates the image and its tooltip appropriately, hiding or showing the icon
  // as needed.
  void Update(zoom::ZoomController* zoom_controller);

 protected:
  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource source) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  // The delegate used to get the currently visible WebContents.
  LocationBarView::Delegate* location_bar_delegate_;

  const gfx::VectorIcon* icon_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ZoomView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_VIEW_H_
