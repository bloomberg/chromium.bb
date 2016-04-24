// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class ContentSettingImageModel;
class LocationBarView;

namespace content {
class WebContents;
}

namespace gfx {
class FontList;
}

namespace views {
class BubbleDialogDelegateView;
class ImageView;
class InkDropDelegate;
class Label;
}

// The ContentSettingImageView displays an icon and optional text label for
// various content settings affordances in the location bar (i.e. plugin
// blocking, geolocation).
class ContentSettingImageView : public IconLabelBubbleView,
                                public gfx::AnimationDelegate,
                                public views::WidgetObserver {
 public:
  // ContentSettingImageView takes ownership of its |image_model|.
  ContentSettingImageView(ContentSettingImageModel* image_model,
                          LocationBarView* parent,
                          const gfx::FontList& font_list,
                          SkColor parent_background_color);
  ~ContentSettingImageView() override;

  // Updates the decoration from the shown WebContents.
  void Update(content::WebContents* web_contents);

 private:
  // Number of milliseconds spent animating open; also the time spent animating
  // closed.
  static const int kOpenTimeMS;

  // The total animation time, including open and close as well as an
  // intervening "stay open" period.
  static const int kAnimationDurationMS;

  // IconLabelBubbleView:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  SkColor GetTextColor() const override;
  SkColor GetBorderColor() const override;
  bool ShouldShowBackground() const override;
  double WidthMultiplier() const override;
  bool IsShrinking() const override;
  bool OnActivate() override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Updates the image and tooltip to match the current model state.
  void UpdateImage();

  LocationBarView* parent_;  // Weak, owns us.
  std::unique_ptr<ContentSettingImageModel> content_setting_image_model_;
  gfx::SlideAnimation slide_animator_;
  bool pause_animation_;
  double pause_animation_state_;
  views::BubbleDialogDelegateView* bubble_view_;

  // This is used to check if the bubble was showing during the mouse pressed
  // event. If this is true then the mouse released event is ignored to prevent
  // the bubble from reshowing.
  bool suppress_mouse_released_action_;

  // Animation delegate for the ink drop ripple effect.
  std::unique_ptr<views::InkDropDelegate> ink_drop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
