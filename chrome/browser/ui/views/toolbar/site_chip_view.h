// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SITE_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SITE_CHIP_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/controls/button/button.h"

class ToolbarView;

namespace content {
class WebContents;
}

namespace gfx {
class Canvas;
}

namespace views {
class Button;
class Label;
}

class SiteChipView : public ToolbarButton,
                     public views::ButtonListener {
 public:
  explicit SiteChipView(ToolbarView* toolbar_view);
  virtual ~SiteChipView();

  void Init();

  // Returns true if the site chip experiment is enabled and thus the site chip
  // should be shown in the toolbar.
  bool ShouldShow();

  // Recalculates the contents of the Site Chip based on the displayed tab.
  void Update(content::WebContents* tab);

  views::ImageView* location_icon_view() { return location_icon_view_; }

  // ToolbarButton:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  ToolbarView* toolbar_view_;
  views::Label* host_label_;
  LocationIconView* location_icon_view_;
  scoped_ptr<views::Painter> background_painter_;

  DISALLOW_COPY_AND_ASSIGN(SiteChipView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SITE_CHIP_VIEW_H_
