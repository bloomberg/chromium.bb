// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SITE_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SITE_CHIP_VIEW_H_

#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/drag_controller.h"

class SiteChipExtensionIcon;
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
                     public views::ButtonListener,
                     public views::DragController,
                     public SafeBrowsingUIManager::Observer {
 public:
  explicit SiteChipView(ToolbarView* toolbar_view);
  virtual ~SiteChipView();

  void Init();

  // Returns true if the site chip experiment is enabled and thus the site chip
  // should be shown in the toolbar.
  bool ShouldShow();

  // Recalculates the contents of the Site Chip based on the displayed tab.
  void Update(content::WebContents* tab);

  // Called to signal that the contents of the tab being shown has changed, so
  // the site chip needs to update itself to the new state.
  void OnChanged();

  views::ImageView* location_icon_view() {
    return location_icon_view_;
  }
  const views::ImageView* location_icon_view() const {
    return location_icon_view_;
  }

  // ToolbarButton:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // SafeBrowsingUIManager::Observer:
  virtual void OnSafeBrowsingHit(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;
  virtual void OnSafeBrowsingMatch(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;

 private:
  string16 SiteLabelFromURL(const GURL& url);

  ToolbarView* toolbar_view_;
  views::Label* host_label_;
  LocationIconView* location_icon_view_;
  scoped_ptr<views::Painter> ev_background_painter_;
  scoped_ptr<views::Painter> broken_ssl_background_painter_;
  scoped_ptr<views::Painter> malware_background_painter_;
  // Will point to one of the background painters, or NULL if the state of the
  // chip has no background.
  views::Painter* painter_;
  bool showing_16x16_icon_;
  scoped_ptr<SiteChipExtensionIcon> extension_icon_;
  GURL url_displayed_;
  ToolbarModel::SecurityLevel security_level_;
  bool url_malware_;

  DISALLOW_COPY_AND_ASSIGN(SiteChipView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SITE_CHIP_VIEW_H_
