// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/status_bubble.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Point;
}
namespace views {
class View;
class Widget;
}

// StatusBubble displays a bubble of text that fades in, hovers over the
// browser chrome and fades away when not needed. It is primarily designed
// to allow users to see where hovered links point to.
class StatusBubbleViews : public StatusBubble {
 public:
  // How wide the bubble's shadow is.
  static const int kShadowThickness;

  // The combined vertical padding above and below the text.
  static const int kTotalVerticalPadding = 7;

  // |base_view| is the view that this bubble is positioned relative to.
  explicit StatusBubbleViews(views::View* base_view);
  virtual ~StatusBubbleViews();

  views::View* base_view() { return base_view_; }

  // Reposition the bubble - as we are using a WS_POPUP for the bubble,
  // we have to manually position it when the browser window moves.
  virtual void Reposition();

  // The bubble only has a preferred height: the sum of the height of
  // the font and kTotalVerticalPadding.
  gfx::Size GetPreferredSize();

  // Set the bounds of the bubble relative to |base_view_|.
  void SetBounds(int x, int y, int w, int h);

  // Set bubble to new width.
  void SetBubbleWidth(int width);

  // Overridden from StatusBubble:
  virtual void SetStatus(const string16& status) OVERRIDE;
  virtual void SetURL(const GURL& url, const std::string& languages) OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void MouseMoved(const gfx::Point& location,
                          bool left_content) OVERRIDE;
  virtual void UpdateDownloadShelfVisibility(bool visible) OVERRIDE;

  views::Widget* GetPopupForTest() { return popup(); }

 protected:
  views::Widget* popup() { return popup_.get(); }

 private:
  class StatusView;
  class StatusViewExpander;

  // Initializes the popup and view.
  void Init();

  // Attempt to move the status bubble out of the way of the cursor, allowing
  // users to see links in the region normally occupied by the status bubble.
  void AvoidMouse(const gfx::Point& location);

  // Returns true if the base_view_'s widget is visible and not minimized.
  bool IsFrameVisible();

  // Expand bubble size to accommodate a long URL.
  void ExpandBubble();

  // Cancel all waiting expansion animations in the timer.
  void CancelExpandTimer();

  // Get the standard width for a status bubble in the current frame size.
  int GetStandardStatusBubbleWidth();

  // Get the maximum possible width for a status bubble in the current frame
  // size.
  int GetMaxStatusBubbleWidth();

  // The status text we want to display when there are no URLs to display.
  string16 status_text_;

  // The url we want to display when there is no status text to display.
  string16 url_text_;

  // The original, non-elided URL.
  GURL url_;

  // Used to elide the original URL again when we expand it.
  std::string languages_;

  // Position relative to the base_view_.
  gfx::Point original_position_;
  // original_position_ adjusted according to the current RTL.
  gfx::Point position_;
  gfx::Size size_;

  // Last location passed to MouseMoved().
  gfx::Point last_mouse_moved_location_;

  // Whether the view contains the mouse.
  bool contains_mouse_;

  // How vertically offset the bubble is from its root position_.
  int offset_;

  // We use a HWND for the popup so that it may float above any HWNDs in our
  // UI (the location bar, for example).
  scoped_ptr<views::Widget> popup_;
  double opacity_;

  views::View* base_view_;
  StatusView* view_;

  // Manages the expansion of a status bubble to fit a long URL.
  scoped_ptr<StatusViewExpander> expand_view_;

  // If the download shelf is visible, do not obscure it.
  bool download_shelf_is_visible_;

  // If the bubble has already been expanded, and encounters a new URL,
  // change size immediately, with no hover.
  bool is_expanded_;

  // Times expansion of status bubble when URL is too long for standard width.
  base::WeakPtrFactory<StatusBubbleViews> expand_timer_factory_;

  DISALLOW_COPY_AND_ASSIGN(StatusBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_BUBBLE_VIEWS_H_
