// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#pragma once

#include "chrome/browser/first_run/first_run.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "views/controls/button/button.h"

class Profile;

class FirstRunBubble : public views::BubbleDelegateView,
                       public views::ButtonListener {
 public:
  static FirstRunBubble* ShowBubble(
      Profile* profile,
      views::View* anchor_view,
      views::BubbleBorder::ArrowLocation location,
      FirstRun::BubbleType bubble_type);

  // views::BubbleDelegateView overrides:
  virtual gfx::Point GetAnchorPoint() OVERRIDE;

 protected:
  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

 private:
  FirstRunBubble(Profile* profile,
                 views::View* anchor_view,
                 views::BubbleBorder::ArrowLocation arrow_location,
                 FirstRun::BubbleType bubble_type);
  virtual ~FirstRunBubble();

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  Profile* profile_;

  FirstRun::BubbleType bubble_type_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
