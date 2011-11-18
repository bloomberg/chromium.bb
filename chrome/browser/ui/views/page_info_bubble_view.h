// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
#pragma once

#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_model_observer.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "views/controls/link_listener.h"

class PageInfoBubbleView : public views::BubbleDelegateView,
                           public PageInfoModelObserver,
                           public views::LinkListener {
 public:
  PageInfoBubbleView(views::View* anchor_view,
                     Profile* profile,
                     const GURL& url,
                     const NavigationEntry::SSLStatus& ssl,
                     bool show_history);
  virtual ~PageInfoBubbleView();

  // Show the certificate dialog.
  void ShowCertDialog();

  // views::View methods:
  virtual gfx::Size GetPreferredSize();

  // PageInfoModelObserver methods:
  virtual void OnPageInfoModelChanged() OVERRIDE;

  // views::BubbleDelegate methods:
  virtual gfx::Point GetAnchorPoint() OVERRIDE;

  // views::LinkListener methods:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // ui::AnimationDelegate methods:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  // Gets the size of the separator, including padding.
  gfx::Size GetSeparatorSize();

  // Get the current value of |resize_animation_| (in [0.0, 1.0]).
  double GetResizeAnimationCurrentValue();

  // Gets the animation value to use for setting the height.
  double HeightAnimationValue();

  // Layout the sections within the bubble.
  void LayoutSections();

  // The model providing the various section info.
  PageInfoModel model_;

  // The id of the certificate for this page.
  int cert_id_;

  // The Help Center link at the bottom of the bubble.
  views::Link* help_center_link_;

  // Animation that helps us change size smoothly as more data comes in.
  ui::SlideAnimation resize_animation_;

  // The height of the info bubble at the start of the resize animation.
  int animation_start_height_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
