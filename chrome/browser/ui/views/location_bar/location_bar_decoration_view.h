// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_DECORATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_DECORATION_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/image_view.h"

////////////////////////////////////////////////////////////////////////////////
//
// LocationBarDecorationView
//
//  An abstract class to provide common functionality to all icons that show up
//  in the omnibox (like the bookmarks star or SSL lock).
//
////////////////////////////////////////////////////////////////////////////////
class LocationBarDecorationView : public views::ImageView {
 public:
  LocationBarDecorationView();
  virtual ~LocationBarDecorationView();

  // views::ImageView:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 protected:
  // Whether this icon should currently be able to process a mouse click. Called
  // both on mouse up and mouse down; must return true both times to for
  // |OnClick()| to be called.
  virtual bool CanHandleClick() const;

  // Called when a user mouses up, taps, or presses a key on this icon.
  virtual void OnClick() = 0;

 private:
  // Set when the user's mouse goes down to determine whether |CanHandleClick()|
  // was true at that point.
  bool could_handle_click_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarDecorationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_DECORATION_VIEW_H_
