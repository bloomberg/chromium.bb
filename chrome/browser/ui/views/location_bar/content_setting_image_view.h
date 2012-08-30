// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget_observer.h"

class ContentSettingImageModel;
class ContentSettingBubbleContents;
class LocationBarView;
class TabContents;

namespace content {
class WebContents;
}

namespace views {
class GestureEvent;
class MouseEvent;
}

namespace ui {
class SlideAnimation;
enum GestureStatus;
}

class ContentSettingsDelegateView;

class ContentSettingImageView : public views::ImageView,
                                public ui::AnimationDelegate,
                                public views::WidgetObserver,
                                public TouchableLocationBarView {
 public:
  ContentSettingImageView(ContentSettingsType content_type,
                          const int background_images[],
                          LocationBarView* parent);
  virtual ~ContentSettingImageView();

  // |new_navigation| true if this is a new navigation, false if the tab was
  // just switched to.
  virtual void Update(TabContents* tab_contents);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual ui::GestureStatus OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  // views::WidgetObserver override:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

 protected:
  // Invoked when the user clicks on the control.
  virtual void OnClick();

 private:
  // views::ImageView overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

  views::Widget* bubble_widget_;

  // The owning LocationBarView.
  LocationBarView* parent_;

  scoped_ptr<ui::SlideAnimation> slide_animator_;
  string16 animated_text_;
  bool pause_animation_;
  int text_size_;
  int visible_text_size_;
  views::HorizontalPainter background_painter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
