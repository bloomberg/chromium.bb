// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "views/view.h"

class ConfirmBubbleModel;

// A class that implements a bubble that consists of the following items:
// * one icon ("icon")
// * one title text ("title")
// * one message text ("message")
// * one optional link ("link")
// * two optional buttons ("ok" and "cancel")
//
// This bubble is convenient when we wish to ask transient, non-blocking
// questions. Unlike a dialog, a bubble menu disappears when we click outside of
// its window to avoid blocking user operations. A bubble is laid out as
// follows:
//
//   +------------------------+
//   | icon title           x |
//   | message                |
//   | link                   |
//   |          [OK] [Cancel] |
//   +------------------------+
//
class ConfirmBubbleView : public BubbleDelegate,
                          public views::ButtonListener,
                          public views::LinkListener,
                          public views::View {
 public:
  explicit ConfirmBubbleView(ConfirmBubbleModel* model);

 protected:
  virtual ~ConfirmBubbleView();

  // BubbleDelegate implementation.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::View implementation.
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

 private:
  // The model to customize this bubble view.
  scoped_ptr<ConfirmBubbleModel> model_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEW_H_
