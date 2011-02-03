// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIEWS_MENU_LOCATOR_H_
#define CHROME_BROWSER_CHROMEOS_VIEWS_MENU_LOCATOR_H_
#pragma once

#include "third_party/skia/include/core/SkScalar.h"

namespace gfx {
class Insets;
class Point;
class Size;
}  // namespace gfx

namespace chromeos {

class WebUIMenuWidget;

// MenuLocator class contorls where the menu will be placed and
// which corners are rounded.
// TODO(oshima): support RTL.
class MenuLocator {
 public:
  enum SubmenuDirection {
    DEFAULT, // default direction.
    RIGHT,   // submenu should grow to right. (not used now. reserved for RTL)
    LEFT,    // submenu should grow to left.
  };

  virtual ~MenuLocator() {}

  // Returns the direction that submenu should grow.
  virtual SubmenuDirection GetSubmenuDirection() const = 0;

  // Move the widget to the right position.
  virtual void Move(WebUIMenuWidget* widget) = 0;

  // Resize and move the widget to the right position.
  virtual void SetBounds(WebUIMenuWidget* widget,
                         const gfx::Size& size) = 0;

  // Returns the 8 length array of SkScalar that represents 4 corner
  // radious for each menu type. The objects are owned by the locator
  // and should not be deleted. This can be null when SubMenu is
  // still off screen (not visible).
  virtual const SkScalar* GetCorners() const = 0;

  // Returns the insets to give space to draw rounded corners.
  virtual void GetInsets(gfx::Insets* insets) const = 0;

  // Returns the menu locator for dropdown menu. The menu will
  // positioned so that the top right corner is given by "point".
  // Only bottom corners are rounded.
  static MenuLocator* CreateDropDownMenuLocator(const gfx::Point& point);

  // Returns the menu locator for context menu. The menu will
  // positioned so that the top left corner is given by "point" (in
  // LTR).  All 4 corners are rounded.
  static MenuLocator* CreateContextMenuLocator(const gfx::Point& point);

  // Returns the menu locator for submenu menu. The menu will
  // positioned at Y and on the right side of the widget, or on the
  // left if there is no enough spaceon the right side. Once it changes the
  // derection, all subsequent submenu should be positioned to the same
  // direction given by |parent_direction|. 3 corners are
  // rounded except for the corner that is attached to the widget.
  static MenuLocator* CreateSubMenuLocator(
      const WebUIMenuWidget* widget,
      MenuLocator::SubmenuDirection parent_direction,
      int y);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VIEWS_MENU_LOCATOR_H_
