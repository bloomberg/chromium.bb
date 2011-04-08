// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/size.h"

class InfoBarContainer;
class InfoBarDelegate;

namespace ui {
class SlideAnimation;
}

class InfoBar : public ui::AnimationDelegate {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() { return delegate_; }
  void set_container(InfoBarContainer* container) { container_ = container; }

  // Makes the infobar visible.  If |animate| is true, the infobar is then
  // animated to full size.
  void Show(bool animate);

  // Makes the infobar hidden.  If |animate| is true, the infobar is first
  // animated to zero size.  Once the infobar is hidden, it is removed from its
  // container (triggering its deletion), and its delegate is closed.
  void Hide(bool animate);

  int tab_height() const { return tab_height_; }
  int total_height() const { return tab_height_ + bar_height_; }

 protected:
  // The target heights of the InfoBar tab and bar portions, regardless of what
  // their current heights are (due to animation).  Platforms must define these!
  static const int kTabTargetHeight;
  static const int kDefaultBarTargetHeight;

  // ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Called when the user closes the infobar, notifies the delegate we've been
  // dismissed and forwards a removal request to our owner.
  void RemoveInfoBar();

  // Changes the target height of the main ("bar") portion of the infobar.
  void SetBarTargetHeight(int height);

  // Given a control with size |prefsize|, returns the centered y position
  // within us, taking into account animation so the control "slides in" (or
  // out) as we animate open and closed.
  int OffsetY(const gfx::Size& prefsize) const;

  ui::SlideAnimation* animation() { return animation_.get(); }
  const ui::SlideAnimation* animation() const { return animation_.get(); }
  int bar_target_height() const { return bar_target_height_; }
  int bar_height() const { return bar_height_; }

  // Platforms may optionally override these if they need to do work during
  // processing of the given calls.
  virtual void PlatformSpecificHide(bool animate) {}
  virtual void PlatformSpecificOnHeightRecalculated() {}

 private:
  // ui::AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Finds the new desired tab and bar heights, and if they differ from the
  // current ones, calls PlatformSpecificOnHeightRecalculated() and informs our
  // container our height has changed.
  void RecalculateHeight();

  // Checks whether we're closed.  If so, notifies the container that it should
  // remove us (which will cause the platform-specific code to asynchronously
  // delete us) and closes the delegate.
  void MaybeDelete();

  InfoBarDelegate* delegate_;
  InfoBarContainer* container_;
  scoped_ptr<ui::SlideAnimation> animation_;

  // The target height for the bar portion of the InfoBarView.
  int bar_target_height_;

  // The current heights of the tab and bar portions.
  int tab_height_;
  int bar_height_;

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_H_
