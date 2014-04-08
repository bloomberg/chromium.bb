// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ORIGIN_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ORIGIN_CHIP_VIEW_H_

#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

class LocationBarView;
class OriginChipExtensionIcon;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Canvas;
class FontList;
class SlideAnimation;
}

namespace views {
class Button;
class Label;
}

class OriginChipView : public views::LabelButton,
                       public views::ButtonListener,
                       public SafeBrowsingUIManager::Observer {
 public:
  OriginChipView(LocationBarView* location_bar_view,
                 Profile* profile,
                 const gfx::FontList& font_list);
  virtual ~OriginChipView();

  // Returns true if the origin chip should be visible.  This will always be
  // true if the original origin chip experiment is enabled.  If the V2
  // experiment is enabled this is true if the chip hasn't been hidden by
  // clicking on it or interacting with the Omnibox.
  bool ShouldShow();

  // Recalculates the contents of the Origin Chip based on the displayed tab.
  void Update(content::WebContents* tab);

  // Called to signal that the contents of the tab being shown has changed, so
  // the origin chip needs to update itself to the new state.
  void OnChanged();

  views::ImageView* location_icon_view() {
    return location_icon_view_;
  }
  const views::ImageView* location_icon_view() const {
    return location_icon_view_;
  }

  // Elides the hostname shown to the indicated width, if needed. Returns the
  // final width of the origin chip. Note: this may be more than the target
  // width, since the hostname will not be elided past the TLD+1.
  int ElideDomainTarget(int target_max_width);

  // Starts an animation that fades in the border.
  void FadeIn();

  // Returns the current X position of the host label.
  int host_label_x() const { return host_label_->x(); }

  // views::LabelButton:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  // Sets an image grid to represent the current security state.
  void SetBorderImages(const int images[3][9]);

  // views::LabelButton:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // SafeBrowsingUIManager::Observer:
  virtual void OnSafeBrowsingHit(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;
  virtual void OnSafeBrowsingMatch(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;

  LocationBarView* location_bar_view_;
  Profile* profile_;
  views::Label* host_label_;
  LocationIconView* location_icon_view_;
  bool showing_16x16_icon_;
  scoped_ptr<OriginChipExtensionIcon> extension_icon_;
  GURL url_displayed_;
  ToolbarModel::SecurityLevel security_level_;
  bool url_malware_;
  scoped_ptr<gfx::SlideAnimation> fade_in_animation_;

  DISALLOW_COPY_AND_ASSIGN(OriginChipView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ORIGIN_CHIP_VIEW_H_
