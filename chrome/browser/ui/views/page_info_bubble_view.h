// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
#pragma once

#include "chrome/browser/page_info_model.h"
#include "chrome/browser/ui/views/info_bubble.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {
class Label;
}

class PageInfoBubbleView : public views::View,
                           public PageInfoModel::PageInfoModelObserver,
                           public InfoBubbleDelegate,
                           public views::LinkController,
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

  void set_info_bubble(InfoBubble* info_bubble) { info_bubble_ = info_bubble; }

  // View methods:
  virtual gfx::Size GetPreferredSize();

  // PageInfoModel::PageInfoModelObserver methods:
  virtual void ModelChanged();

  // InfoBubbleDelegate methods:
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) {}
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();
  virtual std::wstring accessible_name();

  // LinkController methods:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);

 private:
  // Layout the sections within the bubble.
  void LayoutSections();

  // The model providing the various section info.
  PageInfoModel model_;

  // The parent window of the InfoBubble showing this view.
  gfx::NativeWindow parent_window_;

  // The id of the certificate for this page.
  int cert_id_;

  InfoBubble* info_bubble_;

  // The Help Center link at the bottom of the bubble.
  views::Link* help_center_link_;

  // Animation that helps us change size smoothly as more data comes in.
  ui::SlideAnimation resize_animation_;

  // The height of the info bubble at the start of the resize animation.
  int animation_start_height_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_BUBBLE_VIEW_H_
