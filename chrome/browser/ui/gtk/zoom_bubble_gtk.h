// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

class FullscreenController;

namespace content {
class NotificationDetails;
class NotificationSource;
class WebContents;
}

class ZoomBubbleGtk : public content::NotificationObserver {
 public:
  // Shows the zoom bubble below |anchor_widget| with an arrow pointing at
  // |anchor_widget|. If |anchor_widget| is a toplevel window, the bubble will
  // fixed positioned in the top right of corner of the widget with no arrow.
  static void ShowBubble(content::WebContents* web_contents,
                         bool auto_close);

  // Whether the zoom bubble is currently showing.
  static bool IsShowing();

  // Closes the zoom bubble (if there is one).
  static void CloseBubble();

 private:
  ZoomBubbleGtk(GtkWidget* anchor,
                content::WebContents* web_contents,
                bool auto_close,
                FullscreenController* fullscreen_controller);

  virtual ~ZoomBubbleGtk();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Convenience method to start |timer_| if |auto_close_| is true.
  void StartTimerIfNecessary();

  // Stops any close timer if |timer_| is currently running.
  void StopTimerIfNecessary();

  // Refreshes the bubble by changing the zoom percentage appropriately and
  // resetting the timer if necessary.
  void Refresh();

  // Closes the zoom bubble.
  void Close();

  // Notified when the bubble is destroyed so this instance can be deleted.
  CHROMEGTK_CALLBACK_0(ZoomBubbleGtk, void, OnDestroy);

  // Fired when the reset link is clicked.
  CHROMEGTK_CALLBACK_0(ZoomBubbleGtk, void, OnSetDefaultLinkClick);

  // Fired when the mouse enters or leaves the widget.
  CHROMEGTK_CALLBACK_1(ZoomBubbleGtk, gboolean, OnMouseEnter,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(ZoomBubbleGtk, gboolean, OnMouseLeave,
                       GdkEventCrossing*);

  // Whether the currently displayed bubble will automatically close.
  bool auto_close_;

  // Whether the mouse is currently inside the bubble.
  bool mouse_inside_;

  // Timer used to close the bubble when |auto_close_| is true.
  base::OneShotTimer<ZoomBubbleGtk> timer_;

  // The WebContents for the page whose zoom has changed.
  content::WebContents* web_contents_;

  // An event box that wraps the content of the bubble.
  GtkWidget* event_box_;

  // Label showing zoom percentage.
  GtkWidget* label_;

  // The BubbleGtk object containing the zoom bubble's content.
  BubbleGtk* bubble_;

  // Used to register for fullscreen change notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
