// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"

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

 protected:
  // ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Called when the user closes the infobar, notifies the delegate we've been
  // dismissed and forwards a removal request to our owner.
  void RemoveInfoBar();

  ui::SlideAnimation* animation() { return animation_.get(); }
  const ui::SlideAnimation* animation() const { return animation_.get(); }

  // Calls PlatformSpecificRecalculateHeight(), then informs our container our
  // height has changed.
  void RecalculateHeight();

  // Platforms may optionally override these if they need to do work during
  // processing of the given calls.
  virtual void PlatformSpecificHide(bool animate) {}
  virtual void PlatformSpecificRecalculateHeight() {}

 private:
  // ui::AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Checks whether we're closed.  If so, notifies the container that it should
  // remove us (which will cause the platform-specific code to asynchronously
  // delete us) and closes the delegate.
  void MaybeDelete();

  InfoBarDelegate* delegate_;
  InfoBarContainer* container_;
  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_H_
