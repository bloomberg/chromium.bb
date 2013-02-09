// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SCRIPT_BUBBLE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SCRIPT_BUBBLE_ICON_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/views/controls/image_view.h"

class CommandUpdater;

// The ScriptBubbleIconView is the code behind the script bubble icon
// that we show in the Omnibox badged with a number to represent how many
// extensions are running content_scripts in the current page.
class ScriptBubbleIconView : public views::ImageView,
                             public TouchableLocationBarView {
 public:
  explicit ScriptBubbleIconView(
      LocationBarView::Delegate* location_bar_delegate);
  virtual ~ScriptBubbleIconView();

  // TouchableLocationBarView methods.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

  // Updates the number shown on the script bubble icon.
  void SetScriptCount(size_t script_count);

 private:
  // views::View methods.
  virtual void Layout() OVERRIDE;

  // views::ImageView methods.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Shows the script bubble, anchored to anchor_view.
  void ShowScriptBubble(views::View* anchor_view,
                        content::WebContents* web_contents);

  // The location bar that owns us.
  LocationBarView::Delegate* location_bar_delegate_;

  // The last reported script count.
  size_t script_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScriptBubbleIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SCRIPT_BUBBLE_ICON_VIEW_H_
