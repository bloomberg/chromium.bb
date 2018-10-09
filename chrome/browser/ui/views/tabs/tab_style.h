// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/path.h"

class Tab;

// Used to generate a number of outlines for clipping, border painting,
// interior fill, etc. for tabs.
class TabStyle {
 public:
  // The different types of path GetPath() can return. Different paths are used
  // in different situations, but most (excluding |kClip|) are roughly the same
  // shape.
  enum class PathType {
    // Interior fill outline. Extends halfway into the border so there are no
    // gaps between border and fill.
    kFill,
    // Inside path of the border. Border is currently the difference between
    // outside and inside, allowing for a varying thickness of stroke.
    kInsideBorder,
    // Outside path of the border. Border is currently the difference between
    // outside and inside, allowing for a varying thickness of stroke.
    kOutsideBorder,
    // The hit test region. May be extended into a rectangle that touches the
    // top of the bounding box when the window is maximized, for Fitts' Law.
    kHitTest,
    // The area inside the tab where children can be rendered, used to clip
    // child views. Does not have to be the same shape as the border.
    kClip
  };

  // How we want the resulting path scaled.
  enum class RenderUnits {
    // The path is in pixels, and should have its internal area nicely aligned
    // to pixel boundaries.
    kPixels,
    // The path is in DIPs. It will likely be calculated in pixels and then
    // scaled back down.
    kDips
  };

  // If we want to draw vertical separators between tabs, these are the leading
  // and trailing separator stroke rectangles.
  struct SeparatorBounds {
    gfx::RectF leading;
    gfx::RectF trailing;
  };

  // Gets the specific |path_type| associated with the specific |tab|.
  // If |force_active| is true, applies an active appearance on the tab (usually
  // involving painting an optional stroke) even if the tab is not the active
  // tab.
  virtual gfx::Path GetPath(
      const Tab* tab,
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const = 0;

  // Gets the bounds for the leading and trailing separators for a tab.
  virtual SeparatorBounds GetSeparatorBounds(const Tab* tab,
                                             float scale) const = 0;

  // Gets the currently active tab style.
  // TODO(dfried): As we move more of the rendering code into TabStyle from
  // Tab, we may need to have one instance of TabStyle per tab instead of a
  // singleton (e.g. for caching purposes).
  static const TabStyle* GetInstance();

 protected:
  // Avoid implicitly-deleted constructor.
  TabStyle() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStyle);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_H_
