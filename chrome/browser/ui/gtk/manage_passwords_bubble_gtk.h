// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_MANAGE_PASSWORDS_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_MANAGE_PASSWORDS_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"

typedef struct _GtkWidget GtkWidget;

class FullscreenController;

namespace content {
class WebContents;
}

class ManagePasswordsBubbleGtk {
 public:
  // Shows the manage passwords bubble below the |manage_passwords_icon_widget|
  // with an arrow pointing at the icon. If we are in full screen mode the
  // bubble will be positioned in the top right of corner of the widget with no
  // arrow.
  static void ShowBubble(content::WebContents* web_contents);

  // Whether the manage passwords bubble is currently showing.
  static bool IsShowing();

  // Closes the manage passwords bubble (if there is one).
  static void CloseBubble();

 private:
  ManagePasswordsBubbleGtk(GtkWidget* anchor,
                           content::WebContents* web_contents,
                           FullscreenController* fullscreen_controller);

  virtual ~ManagePasswordsBubbleGtk();

  // Closes the manage password bubble.
  void Close();

  // Notified when the bubble is destroyed so this instance can be deleted.
  CHROMEGTK_CALLBACK_0(ManagePasswordsBubbleGtk, void, OnDestroy);

  // Fired when the save button is clicked.
  CHROMEGTK_CALLBACK_0(ManagePasswordsBubbleGtk, void, OnSaveButtonClicked);

  // Fired when the not now button is clicked.
  CHROMEGTK_CALLBACK_0(ManagePasswordsBubbleGtk, void, OnNotNowButtonClicked);

  // The WebContents for the page where the manage passwords bubble is
  // displayed.
  content::WebContents* web_contents_;

  // The BubbleGtk object containing the manage passwords bubble's content.
  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_MANAGE_PASSWORDS_BUBBLE_GTK_H_
