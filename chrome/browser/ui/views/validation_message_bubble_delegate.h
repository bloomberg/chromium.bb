// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_DELEGATE_H_

#include "ui/views/bubble/bubble_delegate.h"

// A BubbleDelegateView implementation for form validation message bubble.
// This class is exposed for testing.
class ValidationMessageBubbleDelegate : public views::BubbleDelegateView {
 public:
  // An interface to observe the widget closing.
  class Observer {
   public:
    virtual void WindowClosing() = 0;

   protected:
    virtual ~Observer() {}
  };

  static const int kWindowMinWidth;
  static const int kWindowMaxWidth;

  ValidationMessageBubbleDelegate(const gfx::Rect& anchor_in_screen,
                                  const string16& main_text,
                                  const string16& sub_text,
                                  Observer* observer);
  virtual ~ValidationMessageBubbleDelegate();

  void Close();
  void SetPositionRelativeToAnchor(const gfx::Rect& anchor_in_screen);

  // BubbleDelegateView overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

 private:
  Observer* observer_;
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageBubbleDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_VALIDATION_MESSAGE_BUBBLE_DELEGATE_H_
