// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_PAINT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_PAINT_H_

#include "base/macros.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class Path;
class Size;
}  // namespace gfx

namespace views {
class View;
}

// A helper class to separate out the painting code from the rest of the
// tab behavior logic.
class TabExperimentalPaint {
 public:
  explicit TabExperimentalPaint(views::View* view);
  ~TabExperimentalPaint();

  // Returns a path corresponding to the tab's outer border for a given tab
  // |size|, |scale|, and |endcap_width|. If |unscale_at_end| is true, this
  // path will be normalized to a 1x scale by scaling by 1/scale before
  // returning. If |extend_to_top| is true, the path is extended vertically to
  // the top of the tab bounds.  The caller uses this for Fitts' Law purposes
  // in maximized/fullscreen mode.
  gfx::Path GetBorderPath(float scale,
                          bool unscale_at_end,
                          bool extend_to_top,
                          float endcap_width,
                          const gfx::Size& size) const;

  void PaintTabBackground(gfx::Canvas* canvas,
                          bool active,
                          int fill_id,
                          int y_offset,
                          gfx::Path* clip);

  void PaintGroupBackground(gfx::Canvas* canvas, bool active);

 private:
  class BackgroundCache {
   public:
    BackgroundCache();
    ~BackgroundCache();

    bool CacheKeyMatches(float scale,
                         const gfx::Size& size,
                         SkColor active_color,
                         SkColor inactive_color,
                         SkColor stroke_color);
    void SetCacheKey(float scale,
                     const gfx::Size& size,
                     SkColor active_color,
                     SkColor inactive_color,
                     SkColor stroke_color);

    // The PaintRecords being cached based on the input parameters.
    sk_sp<cc::PaintRecord> fill_record;
    sk_sp<cc::PaintRecord> stroke_record;

   private:
    // Parameters used to construct the PaintRecords.
    float scale_ = 0.f;
    gfx::Size size_;
    SkColor active_color_ = 0;
    SkColor inactive_color_ = 0;
    SkColor stroke_color_ = 0;

    DISALLOW_COPY_AND_ASSIGN(BackgroundCache);
  };

  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              const gfx::Path& fill_path,
                              bool active,
                              bool paint_hover_effect,
                              SkColor active_color,
                              SkColor inactive_color,
                              int fill_id,
                              int y_offset);
  void PaintTabBackgroundStroke(gfx::Canvas* canvas,
                                const gfx::Path& fill_path,
                                const gfx::Path& stroke_path,
                                bool active,
                                SkColor color);

  views::View* view_;
  gfx::Point background_offset_;

  // Cache of the paint output for tab backgrounds.
  BackgroundCache background_active_cache_;
  BackgroundCache background_inactive_cache_;

  DISALLOW_COPY_AND_ASSIGN(TabExperimentalPaint);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_PAINT_H_
