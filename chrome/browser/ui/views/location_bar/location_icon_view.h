// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

class LocationBarView;

namespace ui {
class KeyEvent;
class LocatedEvent;
}

// Use a LocationIconView to display an icon on the leading side of the edit
// field. It shows the user's current action (while the user is editing), or the
// page security status (after navigation has completed).
class LocationIconView : public IconLabelBubbleView {
 public:
  LocationIconView(const gfx::FontList& font_list,
                   SkColor parent_background_color,
                   LocationBarView* location_bar);
  ~LocationIconView() override;

  // IconLabelBubbleView:
  gfx::Size GetMinimumSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  SkColor GetTextColor() const override;
  SkColor GetBorderColor() const override;
  bool OnActivate() override;

  // Whether we should show the tooltip for this icon or not.
  void set_show_tooltip(bool show_tooltip) { show_tooltip_ = show_tooltip; }

  // Returns what the minimum size would be if the label text were |text|.
  gfx::Size GetMinimumSizeForLabelText(const base::string16& text) const;

  const gfx::FontList& GetFontList() const { return font_list(); }

  // Set the background image. Pass false for |should_show_ev| for all non-EV
  // HTTPS contexts.
  void SetBackground(bool should_show_ev);

 private:
  void ProcessLocatedEvent(const ui::LocatedEvent& event);

  // Returns what the minimum size would be if the preferred size were |size|.
  gfx::Size GetMinimumSizeForPreferredSize(gfx::Size size) const;

  // Handles both click and gesture events by delegating to the page info
  // helper in the appropriate circumstances.
  void OnClickOrTap(const ui::LocatedEvent& event);

  // Set to true when the bubble is already showing at the time the icon is
  // clicked. This suppresses re-showing the bubble on mouse release, so that
  // clicking the icon repeatedly will appear to toggle the bubble on and off.
  bool suppress_mouse_released_action_;

  // True if hovering this view should display a tooltip.
  bool show_tooltip_;

  LocationBarView* location_bar_;

  DISALLOW_COPY_AND_ASSIGN(LocationIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
