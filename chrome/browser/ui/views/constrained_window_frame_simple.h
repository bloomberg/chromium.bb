// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

class ConstrainedWindowViews;

namespace views {
class ImageButton;
class Label;
class LayoutManager;
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView
// Provides a custom Window frame for ConstrainedWindowViews.
// Implements a simple window with a minimal frame.
class ConstrainedWindowFrameSimple : public views::NonClientFrameView,
                                     public views::ButtonListener {
 public:
  // Contains references to relevant views in the header.  The header
  // must be non-NULL.
  struct HeaderViews {
    HeaderViews(views::View* header,
                views::Label* title_label,
                views::Button* close_button);

    views::View* header;
    views::Label* title_label;
    views::Button* close_button;
  };

  explicit ConstrainedWindowFrameSimple(ConstrainedWindowViews* container);
  virtual ~ConstrainedWindowFrameSimple();

  // SetHeaderView assumes ownership of the passed parameter.
  void SetHeaderView(HeaderViews* header_views);

 private:
  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from ButtonListener
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  HeaderViews* CreateDefaultHeaderView();

  views::ImageButton* CreateCloseButton();

  ConstrainedWindowViews* container_;

  views::LayoutManager* layout_;

  scoped_ptr<HeaderViews> header_views_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowFrameSimple);
};
#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_
