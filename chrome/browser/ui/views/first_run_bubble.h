// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#pragma once

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Browser;
class Profile;

class FirstRunBubble : public views::BubbleDelegateView,
                       public views::LinkListener {
 public:
  // |browser| is the opening browser and is NULL in unittests.
  static FirstRunBubble* ShowBubble(Browser* browser,
                                    Profile* profile,
                                    views::View* anchor_view);

  // views::BubbleDelegateView overrides:
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

 protected:
  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

 private:
  FirstRunBubble(Browser* browser, Profile* profile, views::View* anchor_view);
  virtual ~FirstRunBubble();

  // views::LinkListener overrides:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  Browser* browser_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
