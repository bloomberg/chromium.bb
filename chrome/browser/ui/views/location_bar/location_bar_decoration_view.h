// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_DECORATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_DECORATION_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/events/event.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

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
}

// A base class for location bar decorations showing icons and label animations.
// Used for content settings and web intents button. Class is owned by the
// LocationBarView.
class LocationBarDecorationView : public views::ImageView,
                                  public ui::AnimationDelegate,
                                  public TouchableLocationBarView {
 public:
  // |background_images| is the array of images used to draw
  // the label animation background (if any).
  LocationBarDecorationView(LocationBarView* parent,
                            const int background_images[]);
  virtual ~LocationBarDecorationView();

  // Update the decoration from the shown TabContents.
  virtual void Update(TabContents* tab_contents) = 0;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual ui::EventResult OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

 protected:
  // Invoked when the user clicks on the control.
  virtual void OnClick(LocationBarView* parent) = 0;

  // Start animating the text label if it is not already happening.
  virtual void StartLabelAnimation(string16 animated_text, int duration_ms);
  virtual int GetTextAnimationSize(double state, int text_size);

  void PauseAnimation();
  void UnpauseAnimation();

  // Call to always draw the text by the (optional) icon. Used to draw the web
  // intents button.
  void AlwaysDrawText();

 private:
  // views::ImageView overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // Notify the possibly-running animation that it was clicked.
  void AnimationOnClick();

  // The owning LocationBarView. Weak pointer.
  LocationBarView* parent_;

  scoped_ptr<ui::SlideAnimation> slide_animator_;
  string16 animated_text_;
  bool pause_animation_;
  double pause_animation_state_;
  int text_size_;
  int visible_text_size_;
  bool force_draw_text_;
  views::HorizontalPainter background_painter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationBarDecorationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_DECORATION_VIEW_H_
