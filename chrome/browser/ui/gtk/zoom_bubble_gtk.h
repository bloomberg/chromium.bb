// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class TabContents;

typedef struct _GtkWidget GtkWidget;

class ZoomBubbleGtk {
 public:
  // Shows the zoom bubble, pointing at |anchor_widget|.
  static void Show(GtkWidget* anchor, TabContents* tab_contents,
      bool auto_close);

  // Closes the zoom bubble.
  static void Close();

 private:
  ZoomBubbleGtk(GtkWidget* anchor, TabContents* tab_contents, bool auto_close);
  virtual ~ZoomBubbleGtk();

  // Refreshes the bubble by changing the zoom percentage appropriately and
  // resetting the timer if necessary.
  void Refresh();

  // Closes the zoom bubble.
  void CloseBubble();

  // Notified when the bubble is destroyed so this instance can be deleted.
  CHROMEGTK_CALLBACK_0(ZoomBubbleGtk, void, OnDestroy);

  // Fired when the reset link is clicked.
  CHROMEGTK_CALLBACK_0(ZoomBubbleGtk, void, OnSetDefaultLinkClick);

  // Whether the currently displayed bubble will automatically close.
  bool auto_close_;

  // Timer used to close the bubble when |auto_close_| is true.
  base::OneShotTimer<ZoomBubbleGtk> timer_;

  // The TabContents for the page whose zoom has changed.
  TabContents* tab_contents_;

  // Label showing zoom percentage.
  GtkWidget* label_;

  // The BubbleGtk object containing the zoom bubble's content.
  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
