// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_
#define CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_

#include "base/time.h"
#include "chrome/browser/views/info_bubble.h"
#include "views/controls/button/image_button.h"

class BubblePositioner;
class GURL;
class Profile;

namespace views {
class ButtonListener;
class View;
}  // namespace views

// ToolbarStarToggle is used for the star button on the toolbar, allowing the
// user to star the current page. ToolbarStarToggle manages showing the
// InfoBubble and rendering the appropriate state while the bubble is visible.

class ToolbarStarToggle : public views::ToggleImageButton,
                          public InfoBubbleDelegate {
 public:
  explicit ToolbarStarToggle(views::ButtonListener* listener);

  void set_profile(Profile* profile) { profile_ = profile; }
  void set_host_view(views::View* host_view) { host_view_ = host_view; }
  void set_bubble_positioner(BubblePositioner* bubble_positioner) {
    bubble_positioner_ = bubble_positioner;
  }

  // Sets up all labels for the button.
  void Init();

  // Sets up all images for the button.
  void LoadImages();

  // If the bubble isn't showing, shows it.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  // Overridden to update ignore_click_ based on whether the mouse was clicked
  // quickly after the bubble was hidden.
  virtual bool OnMousePressed(const views::MouseEvent& e);

  // Overridden to set ignore_click_ to false.
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  virtual void OnDragDone();

 protected:
  // Only invokes super if ignore_click_ is true and the bubble isn't showing.
  virtual void NotifyClick(const views::Event& event);

  // Overridden to so that we appear pressed while the bubble is showing.
  virtual SkBitmap GetImageToPaint();

 private:
  // InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();

  // Profile with bookmarks info.
  Profile* profile_;

  // View that hosts us.
  views::View* host_view_;

  // Positioner for bookmark bubble.
  BubblePositioner* bubble_positioner_;

  // Time the bubble last closed.
  base::TimeTicks bubble_closed_time_;

  // If true NotifyClick does nothing. This is set in OnMousePressed based on
  // the amount of time between when the bubble clicked and now.
  bool ignore_click_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarStarToggle);
};

#endif  // CHROME_BROWSER_VIEWS_TOOLBAR_STAR_TOGGLE_H_
