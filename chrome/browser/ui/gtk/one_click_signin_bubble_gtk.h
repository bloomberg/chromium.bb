// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_BUBBLE_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class BrowserWindowGtk;

// Displays the one-click signin confirmation bubble (after syncing
// has started).
class OneClickSigninBubbleGtk : public BubbleDelegateGtk {
 public:
  // Deletes self on close.  The given callbacks will be called if the
  // user clicks the corresponding link.
  OneClickSigninBubbleGtk(BrowserWindowGtk* browser_window_gtk,
                          const base::Closure& learn_more_callback,
                          const base::Closure& advanced_callback);

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(
      BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  void ClickOKForTest();
  void ClickLearnMoreForTest();
  void ClickAdvancedForTest();

 private:
  virtual ~OneClickSigninBubbleGtk();

  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickLearnMoreLink);
  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickAdvancedLink);
  CHROMEGTK_CALLBACK_0(OneClickSigninBubbleGtk, void, OnClickOK);

  BubbleGtk* bubble_;
  const base::Closure learn_more_callback_;
  const base::Closure advanced_callback_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_BUBBLE_GTK_H_
