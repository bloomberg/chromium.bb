// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_MEDIA_INDICATOR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_MEDIA_INDICATOR_BUTTON_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class Animation;
class AnimationDelegate;
}

// This is an ImageButton subclass that serves as both the media indicator icon
// (audio, tab capture, etc.), and as a mute button.  It is meant to only be
// used as a child view of Tab.
//
// When the indicator is transitioned to the audio playing or muting state, the
// button functionality is enabled and begins handling mouse events.  Otherwise,
// this view behaves like an image and all mouse events will be handled by the
// Tab (its parent View).
class MediaIndicatorButton : public views::ImageButton,
                             public views::ViewTargeterDelegate {
 public:
  // The MediaIndicatorButton's class name.
  static const char kViewClassName[];

  MediaIndicatorButton();
  virtual ~MediaIndicatorButton();

  // Returns the current TabMediaState except, while the indicator image is
  // fading out, returns the prior TabMediaState.
  TabMediaState showing_media_state() const {
    return showing_media_state_;
  }

  // Updates ImageButton images, starts fade animations, and
  // activates/deactivates button functionality as appropriate.
  void TransitionToMediaState(TabMediaState next_state);

 protected:
  // views::View:
  virtual const char* GetClassName() const OVERRIDE;
  virtual View* GetTooltipHandlerForPoint(const gfx::Point& point) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::ViewTargeterDelegate
  virtual bool DoesIntersectRect(const View* target,
                                 const gfx::Rect& rect) const OVERRIDE;

  // views::Button:
  virtual void NotifyClick(const ui::Event& event) OVERRIDE;

 private:
  class FadeAnimationDelegate;

  TabMediaState media_state_;

  // Media indicator fade-in/out animation (i.e., only on show/hide, not a
  // continuous animation).
  scoped_ptr<gfx::AnimationDelegate> fade_animation_delegate_;
  scoped_ptr<gfx::Animation> fade_animation_;
  TabMediaState showing_media_state_;

  DISALLOW_COPY_AND_ASSIGN(MediaIndicatorButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_MEDIA_INDICATOR_BUTTON_H_
