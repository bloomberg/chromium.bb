// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

typedef struct _GtkWidget GtkWidget;

class ZoomBubbleGtk : public BubbleDelegateGtk,
                      public content::NotificationObserver {
 public:
  // Shows the zoom bubble, pointing at |anchor_widget|.
  static void Show(GtkWidget* anchor, Profile* profile,
    int zoomPercent, bool auto_close);

  // Closes the zoom bubble.
  static void Close();

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  ZoomBubbleGtk(GtkWidget* anchor,
                Profile* profile,
                int zoom_percent,
                bool auto_close);
  virtual ~ZoomBubbleGtk();

  // Closes the zoom bubble.
  void CloseBubble();

  // Notified when |content_| is destroyed so this instance can be deleted.
  CHROMEGTK_CALLBACK_0(ZoomBubbleGtk, void, OnDestroy);

  // Whether the currently displayed bubble will automatically close.
  bool auto_close_;

  // Timer used to close the bubble when |auto_close_| is true.
  base::OneShotTimer<ZoomBubbleGtk> timer_;

  // Provides colors.
  GtkThemeService* theme_service_;

  // Label showing zoom percentage.
  GtkWidget* label_;

  // The BubbleGtk object containing the zoom bubble's content.
  BubbleGtk* bubble_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ZOOM_BUBBLE_GTK_H_
