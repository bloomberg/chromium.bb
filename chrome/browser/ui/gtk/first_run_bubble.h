// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the First Run bubble, the dialog box
// presented on first run of Chromium. There can only ever be a single
// bubble open, so the class presents only static methods.

#ifndef CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/gtk/info_bubble_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;

class FirstRunBubble : public InfoBubbleGtkDelegate,
                       public NotificationObserver {
 public:
  // Shows the first run bubble, pointing at |rect|.
  static void Show(Profile* profile,
                   GtkWidget* anchor,
                   const gfx::Rect& rect,
                   FirstRun::BubbleType bubble_type);

  // Implements the InfoBubbleGtkDelegate.  We are notified when the bubble
  // is about to be closed.
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  FirstRunBubble(Profile* profile,
                 GtkWidget* anchor,
                 const gfx::Rect& rect,
                 FirstRun::BubbleType bubble_type);
  virtual ~FirstRunBubble();

  // Create and pack widgets for different bubble types.
  void InitializeContentForLarge();
  void InitializeContentForOEM();
  void InitializeContentForMinimal();

  // Contains some common set up for the labels in the bubble. |width| is a
  // resource that holds the desired width for the labels.
  void InitializeLabels(int width_resource);

  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleDestroy);
  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleKeepButton);
  CHROMEGTK_CALLBACK_0(FirstRunBubble, void, HandleChangeButton);

  // Our current profile.
  Profile* profile_;

  // Provides colors and stuff.
  GtkThemeProvider* theme_provider_;

  // The widget we anchor to, and a descendant of the toplevel window we
  // are transient for.
  GtkWidget* anchor_;

  // We let the InfoBubble own our content, and then we delete ourself
  // when the widget is destroyed (when the InfoBubble is destroyed).
  GtkWidget* content_;

  // The various labels in the interface. We keep track of them for theme
  // changes.
  std::vector<GtkWidget*> labels_;

  InfoBubbleGtk* bubble_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_UI_GTK_FIRST_RUN_BUBBLE_H_
