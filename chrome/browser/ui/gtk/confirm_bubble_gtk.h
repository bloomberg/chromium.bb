// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CONFIRM_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_CONFIRM_BUBBLE_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/native_widget_types.h"

typedef struct _GtkWidget GtkWidget;

class ConfirmBubbleModel;
class CustomDrawButton;

// A class that implements a bubble that consists of the following items:
// * one icon ("icon")
// * one title text ("title")
// * one close button ("x")
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
//   |          [Cancel] [OK] |
//   +------------------------+
//
class ConfirmBubbleGtk : public BubbleDelegateGtk {
 public:
  ConfirmBubbleGtk(gfx::NativeView parent,
                   const gfx::Point& anchor_point,
                   ConfirmBubbleModel* model);
  virtual ~ConfirmBubbleGtk();

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // Create a bubble and show it.
  void Show();

 private:
  FRIEND_TEST_ALL_PREFIXES(ConfirmBubbleGtkTest, ClickCancel);
  FRIEND_TEST_ALL_PREFIXES(ConfirmBubbleGtkTest, ClickOk);
  FRIEND_TEST_ALL_PREFIXES(ConfirmBubbleGtkTest, ClickLink);

  // GTK event handlers.
  CHROMEGTK_CALLBACK_0(ConfirmBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(ConfirmBubbleGtk, void, OnCloseButton);
  CHROMEGTK_CALLBACK_0(ConfirmBubbleGtk, void, OnOkButton);
  CHROMEGTK_CALLBACK_0(ConfirmBubbleGtk, void, OnCancelButton);
  CHROMEGTK_CALLBACK_0(ConfirmBubbleGtk, void, OnLinkButton);

  // The bubble.
  BubbleGtk* bubble_;

  // The anchor window and the screen point where this bubble is anchored. This
  // class shows a bubble under this point.
  gfx::NativeView anchor_;
  gfx::Point anchor_point_;

  // The model to customize this bubble view.
  scoped_ptr<ConfirmBubbleModel> model_;

  // The x that closes this bubble.
  scoped_ptr<CustomDrawButton> close_button_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_CONFIRM_BUBBLE_GTK_H_
