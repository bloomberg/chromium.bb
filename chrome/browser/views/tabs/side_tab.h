// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_

#include "app/slide_animation.h"
#include "chrome/browser/views/tabs/base_tab_renderer.h"
#include "gfx/font.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class SideTab;
class TabStripController;

class SideTab : public BaseTabRenderer,
                public views::ContextMenuController,
                public views::ButtonListener,
                public AnimationDelegate {
 public:
  explicit SideTab(TabController* controller);
  virtual ~SideTab();

  // AnimationDelegate implementation:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::ContextMenuController implementation:
  virtual void ShowContextMenu(views::View* source,
                               const gfx::Point& p,
                               bool is_mouse_gesture);

  // views::View Overrides:
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);

 protected:
  // BaseTabRenderer overrides.
  virtual void AdvanceLoadingAnimation(TabRendererData::NetworkState state);

 private:
  void FillTabShapePath(gfx::Path* path);

  // Paint various components of the tab.
  void PaintIcon(gfx::Canvas* canvas);
  void PaintLoadingAnimation(gfx::Canvas* canvas);

  // Loads class-specific resources.
  static void InitClass();

  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;

  views::Button* close_button_;

  // Hover animation.
  scoped_ptr<SlideAnimation> hover_animation_;

  static gfx::Font* font_;
  static SkBitmap* close_button_n_;
  static SkBitmap* close_button_m_;
  static SkBitmap* close_button_h_;
  static SkBitmap* close_button_p_;

  // The current index into the Animation image strip.
  int loading_animation_frame_;

  DISALLOW_COPY_AND_ASSIGN(SideTab);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_
