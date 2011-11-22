// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_H_
#define CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "views/view.h"

class PanelScrollerHeader;

class PanelScroller : public views::View, public ui::AnimationDelegate {
 public:
  PanelScroller();
  virtual ~PanelScroller();

  static PanelScroller* CreateWindow();

  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;

  // Called when a panel header is clicked with the affected container. This
  // function will make sure the panel is fully visible.
  void HeaderClicked(PanelScrollerHeader* source);

 private:
  struct Panel;

  // ui::AnimationDelegate overrides.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Scrolls to the panel at the given index. It will be moved to the top.
  void ScrollToPanel(int index);

  // All panels in this scroller.
  std::vector<Panel*> panels_;

  // Height in pixels of the headers above each panel.
  int divider_height_;

  bool needs_layout_;

  // The current scroll position.
  int scroll_pos_;

  ui::SlideAnimation animation_;

  // When animating a scroll, these indicate the beginning and ending of the
  // scroll. The scroll_pos_ always indicates the current one.
  int animated_scroll_begin_;
  int animated_scroll_end_;

  DISALLOW_COPY_AND_ASSIGN(PanelScroller);
};

#endif  // CHROME_BROWSER_CHROMEOS_PANELS_PANEL_SCROLLER_H_
