// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_model_observer.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "views/controls/link_listener.h"
#include "views/view.h"

class PageInfoBubbleView : public views::View,
                           public PageInfoModelObserver,
                           public BubbleDelegate,
                           public views::LinkListener,
                           public ui::AnimationDelegate {
 public:
  PageInfoBubbleView(gfx::NativeWindow parent_window,
                     Profile* profile,
                     const GURL& url,
                     const NavigationEntry::SSLStatus& ssl,
                     bool show_history);
  virtual ~PageInfoBubbleView();

  // Show the certificate dialog.
  void ShowCertDialog();

  void set_bubble(Bubble* bubble) { bubble_ = bubble; }

  // views::View methods:
  virtual gfx::Size GetPreferredSize();

  // PageInfoModelObserver methods:
  virtual void OnPageInfoModelChanged() OVERRIDE;

  // BubbleDelegate methods:
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;
  virtual string16 GetAccessibleName() OVERRIDE;

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

  // Global pointer to the bubble that is hosting our view.
  static Bubble* bubble_;

  // The model providing the various section info.
  PageInfoModel model_;

  // The parent window of the Bubble showing this view.
  gfx::NativeWindow parent_window_;

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
