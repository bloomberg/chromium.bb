// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

class BrowserView;

// Container for the BrowserView's tab strip, toolbar, and sometimes bookmark
// bar. In Chrome OS immersive fullscreen it stacks on top of other views in
// order to slide in and out over the web contents. It informs the immersive
// mode controller when its children lose focus to trigger a slide out.
class TopContainerView : public views::View,
                         public views::FocusChangeListener {
 public:
  explicit TopContainerView(BrowserView* browser_view);
  virtual ~TopContainerView();

  // views::View overrides:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

  // views::FocusChangeListener overrides:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

 private:
  // The parent of this view. Not owned.
  BrowserView* browser_view_;
  // The focus manager of |browser_view_|, cached to allow listener cleanup
  // during |browser_view_| destruction.
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(TopContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_VIEW_H_
