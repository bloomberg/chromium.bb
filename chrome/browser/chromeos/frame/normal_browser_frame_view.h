// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_NORMAL_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_NORMAL_BROWSER_FRAME_VIEW_H_

#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/views/tab_icon_view.h"
#include "views/controls/button/button.h"
#include "views/window/non_client_view.h"

class BaseTabStrip;
class BrowserView;
namespace gfx {
class Font;
}
class TabContents;
namespace views {
class ImageButton;
class ImageView;
}

namespace chromeos {

class NormalBrowserFrameView : public BrowserNonClientFrameView,
                               public TabIconView::TabIconViewModel {
 public:
  // Constructs a non-client view for an BrowserFrame.
  NormalBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~NormalBrowserFrameView();

  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(BaseTabStrip* tabstrip) const;
  virtual void UpdateThrobber(bool running);
  virtual gfx::Size GetMinimumSize();

 protected:
  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual bool AlwaysUseNativeFrame() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual bool HitTest(const gfx::Point& l) const;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);

  // Overridden from TabIconView::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const;
  virtual SkBitmap GetFavIconForTabIconView();

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Paint various sub-components of this view.  The *FrameBorder() functions
  // also paint the background of the titlebar area, since the top frame border
  // and titlebar background are a contiguous component.
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  void PaintToolbarBackground(gfx::Canvas* canvas);

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  DISALLOW_COPY_AND_ASSIGN(NormalBrowserFrameView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_NORMAL_BROWSER_FRAME_VIEW_H_
