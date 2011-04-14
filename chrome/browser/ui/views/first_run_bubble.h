// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/task.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/views/bubble/bubble.h"

class FirstRunBubbleViewBase;
class Profile;

class FirstRunBubble : public Bubble,
                       public BubbleDelegate {
 public:
  static FirstRunBubble* Show(Profile* profile, views::Widget* parent,
                              const gfx::Rect& position_relative_to,
                              BubbleBorder::ArrowLocation arrow_location,
                              FirstRun::BubbleType bubble_type);

 private:
  FirstRunBubble();
  virtual ~FirstRunBubble();

  void set_view(FirstRunBubbleViewBase* view) { view_ = view; }

  // Re-enable the parent window.
  void EnableParent();

#if defined(OS_WIN)
  // Overridden from Bubble:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
#endif

  // BubbleDelegate.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return true; }

  // Whether we have already been activated.
  bool has_been_activated_;

  ScopedRunnableMethodFactory<FirstRunBubble> enable_window_method_factory_;

  // The view inside the FirstRunBubble.
  FirstRunBubbleViewBase* view_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
