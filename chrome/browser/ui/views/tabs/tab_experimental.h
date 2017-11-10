// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_

#include "base/macros.h"
#include "ui/views/controls/glow_hover_controller.h"
#include "ui/views/view.h"

namespace gfx {
class Path;
}

namespace views {
class Label;
}

class TabExperimental : public views::View {
 public:
  TabExperimental();
  ~TabExperimental() override;

  // FIXME(brettw) do we want an equivalent to TabRendererData to set
  // everything at once?
  void SetTitle(const base::string16& title);

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

  void PaintTabBackground(gfx::Canvas* canvas,
                          bool active,
                          int fill_id,
                          int y_offset,
                          gfx::Path* clip);
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              const gfx::Path& fill_path,
                              bool active,
                              bool hover,
                              SkColor active_color,
                              SkColor inactive_color,
                              int fill_id,
                              int y_offset);
  void PaintTabBackgroundStroke(gfx::Canvas* canvas,
                                const gfx::Path& fill_path,
                                const gfx::Path& stroke_path,
                                bool active,
                                SkColor color);

  class BackgroundCache {
   public:
    BackgroundCache();
    ~BackgroundCache();

    bool CacheKeyMatches(float scale,
                         const gfx::Size& size,
                         SkColor active_color,
                         SkColor inactive_color,
                         SkColor stroke_color) {
      return scale_ == scale && size_ == size &&
             active_color_ == active_color &&
             inactive_color_ == inactive_color && stroke_color_ == stroke_color;
    }

    void SetCacheKey(float scale,
                     const gfx::Size& size,
                     SkColor active_color,
                     SkColor inactive_color,
                     SkColor stroke_color) {
      scale_ = scale;
      size_ = size;
      active_color_ = active_color;
      inactive_color_ = inactive_color;
      stroke_color_ = stroke_color;
    }

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
  };

  // Cache of the paint output for tab backgrounds.
  BackgroundCache background_active_cache_;
  BackgroundCache background_inactive_cache_;

  views::Label* title_;  // Non-owning (owned by View hierarchy).

  views::GlowHoverController hover_controller_;
  gfx::Point background_offset_;

  DISALLOW_COPY_AND_ASSIGN(TabExperimental);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_
