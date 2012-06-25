// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the First Run bubble, the dialog box
// presented on first run of Chromium. There can only ever be a single
// bubble open, so the class presents only static methods.

#ifndef CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"

class Browser;

class FirstRunBubble : public BubbleDelegateGtk {
 public:
  // Shows the first run bubble, pointing at |rect|, transient for the toplevel
  // window of the |anchor| widget.
  static void Show(Browser* browser, GtkWidget* anchor, const gfx::Rect& rect);

  // Overridden from BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  FirstRunBubble(Browser* browser, GtkWidget* anchor, const gfx::Rect& rect);
  virtual ~FirstRunBubble();

  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleDestroy);
  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleChangeLink);

  Browser* browser_;
  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
