// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/bubble/bubble.h"
#include "views/controls/button/button.h"

class Browser;
class GlobalError;

class GlobalErrorBubbleView : public views::View,
                              public views::ButtonListener,
                              public BubbleDelegate {
 public:
  GlobalErrorBubbleView(Browser* browser, GlobalError* error);
  virtual ~GlobalErrorBubbleView();

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // BubbleDelegate implementation.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;

 private:
  Browser* browser_;
  GlobalError* error_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
