// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/font.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class ContentSettingImageModel;
class LocationBarView;

namespace content {
class WebContents;
}

namespace ui {
class SlideAnimation;
}

namespace views {
class ImageView;
class Label;
}

// The ContentSettingImageView displays an icon and optional text label for
// various content settings affordances in the location bar (i.e. plugin
// blocking, geolocation).
class ContentSettingImageView : public views::View,
                                public ui::AnimationDelegate,
                                public TouchableLocationBarView,
                                public views::WidgetObserver {
 public:
  // |background_images| is the array of images used to draw
  // the label animation background (if any).
  ContentSettingImageView(ContentSettingsType content_type,
                          const int background_images[],
                          LocationBarView* parent,
                          const gfx::Font& font,
                          int font_y_offset,
                          SkColor font_color);
  virtual ~ContentSettingImageView();

  // Update the decoration from the shown WebContents.
  void Update(content::WebContents* web_contents);

  void SetImage(const gfx::ImageSkia* image_skia);
  void SetTooltipText(const string16& tooltip);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

  // views::WidgetObserver override:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  // Invoked when the user clicks on the control.
  void OnClick(LocationBarView* parent);

  // Start animating the text label if it is not already happening.
  void StartLabelAnimation(string16 animated_text, int duration_ms);
  int GetTextAnimationSize(double state, int text_size);

  void PauseAnimation();
  void UnpauseAnimation();

  // Call to always draw the text by the (optional) icon. Used to draw the web
  // intents button.
  void AlwaysDrawText();

  // views::View overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // Notify the possibly-running animation that it was clicked.
  void AnimationOnClick();

  // The owning LocationBarView. Weak pointer.
  LocationBarView* parent_;

  // Child views that comprise the bubble.
  views::Label* text_label_;
  views::ImageView* icon_;

  scoped_ptr<ui::SlideAnimation> slide_animator_;
  gfx::Font font_;
  SkColor font_color_;
  bool pause_animation_;
  double pause_animation_state_;
  int text_size_;
  int visible_text_size_;
  bool force_draw_text_;
  views::HorizontalPainter background_painter_;
  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;
  views::Widget* bubble_widget_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
