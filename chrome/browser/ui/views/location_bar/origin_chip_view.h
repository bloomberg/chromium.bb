// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ORIGIN_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ORIGIN_CHIP_VIEW_H_

#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

class LocationBarView;
class OriginChipExtensionIcon;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class FontList;
}

namespace views {
class Label;
}

// A button visible at the beginning of the omnibox which contains the location
// icon, an optional EV cert name, and a hostname.  The hostname is normally the
// hostname for the current page with any leading "www." removed, though for
// special built-in pages (e.g. chrome://settings), it can be a descriptive
// string.  The EV cert name is the organization name of the EV cert holder and
// is only present when the current page's security status is EV_SECURE.
class OriginChipView : public views::LabelButton,
                       public views::ButtonListener,
                       public SafeBrowsingUIManager::Observer {
 public:
  OriginChipView(LocationBarView* location_bar_view,
                 Profile* profile,
                 const gfx::FontList& font_list);
  virtual ~OriginChipView();

  SkColor pressed_text_color() const { return pressed_text_color_; }
  SkColor pressed_background_color() const {
    return background_colors_[Button::STATE_PRESSED];
  }
  const base::string16& host_label_text() const { return host_label_->text(); }

  // Called to signal that the contents of the tab being shown has changed, so
  // the origin chip needs to update itself to the new state.
  void OnChanged();

  // Starts/stops a fade-in animation for the border.
  void FadeIn();
  void CancelFade();

  // Returns the offset of the host label, relative to where the first label
  // starts.  When the EV cert name is not visible, this will always be 0;
  // otherwise, it's a positive value equal to the width of the cert name plus
  // the space between the labels.
  int HostLabelOffset() const;

  // Returns the width of the origin chip from the start of the first label to
  // the trailing edge of the chip.
  int WidthFromStartOfLabels() const;

  // views::LabelButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  // Returns the X coordinate the first label should be placed at.
  int GetLabelX() const;

  // Sets an image grid to represent the current security state.
  void SetBorderImages(const int images[3][9]);

  // views::LabelButton:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void StateChanged() OVERRIDE;

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
  SkColor pressed_text_color_;
  SkColor background_colors_[3];
  views::Label* ev_label_;
  views::Label* host_label_;
  LocationIconView* location_icon_view_;
  bool showing_16x16_icon_;
  scoped_ptr<OriginChipExtensionIcon> extension_icon_;
  gfx::SlideAnimation fade_in_animation_;
  GURL url_displayed_;
  ToolbarModel::SecurityLevel security_level_;
  bool url_malware_;

  DISALLOW_COPY_AND_ASSIGN(OriginChipView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ORIGIN_CHIP_VIEW_H_
