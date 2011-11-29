// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FULLSCREEN_EXIT_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_FULLSCREEN_EXIT_BUBBLE_VIEWS_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/fullscreen_exit_bubble.h"

class GURL;
namespace ui {
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

// FullscreenExitBubbleViews is responsible for showing a bubble atop the
// screen in fullscreen mode, telling users how to exit and providing a click
// target. The bubble auto-hides, and re-shows when the user moves to the
// screen top.
class FullscreenExitBubbleViews : public FullscreenExitBubble {
 public:
  FullscreenExitBubbleViews(views::Widget* frame,
                            Browser* browser,
                            const GURL& url,
                            FullscreenExitBubbleType bubble_type);
  virtual ~FullscreenExitBubbleViews();

  void UpdateContent(const GURL& url, FullscreenExitBubbleType bubble_type);

 private:
  class FullscreenExitView;

  // FullScreenExitBubble
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual gfx::Rect GetPopupRect(bool ignore_animation_state) const OVERRIDE;
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual bool WindowContainsPoint(gfx::Point pos) OVERRIDE;
  virtual bool IsWindowActive() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual bool IsAnimating() OVERRIDE;

  void StartWatchingMouseIfNecessary();

  // The root view containing us.
  views::View* root_view_;

  views::Widget* popup_;

  // Animation controlling sliding into/out of the top of the screen.
  scoped_ptr<ui::SlideAnimation> size_animation_;

  // The contents of the popup.
  FullscreenExitView* view_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenExitBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FULLSCREEN_EXIT_BUBBLE_VIEWS_H_
