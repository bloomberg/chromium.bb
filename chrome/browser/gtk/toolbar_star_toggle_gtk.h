// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_
#define CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

class BrowserToolbarGtk;
class GtkThemeProvider;
class GURL;

// Displays the bookmark star button, which toggles between two images.
class ToolbarStarToggleGtk : public NotificationObserver {
 public:
  explicit ToolbarStarToggleGtk(BrowserToolbarGtk* host);
  ~ToolbarStarToggleGtk();

  // Provide NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // If the bubble isn't showing, shows it above the star button.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  void SetStarred(bool starred);

  GtkWidget* widget() const { return widget_.get(); }

 private:
  // Updates the properties of |widget_| when we would need to change its
  // state.
  void UpdateGTKButton();

  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           ToolbarStarToggleGtk* obj);

  // Callback to get the data associated with a drag.
  static void OnDragDataGet(GtkWidget* widget,
                            GdkDragContext* drag_context,
                            GtkSelectionData* data,
                            guint info,
                            guint time,
                            ToolbarStarToggleGtk* star);

  // Used to listen for theme change notifications.
  NotificationRegistrar registrar_;

  // The browser toolbar hosting this widget, for getting the current profile.
  BrowserToolbarGtk* host_;

  // The actual button widget.
  OwnedWidgetGtk widget_;

  // Whether we show the yellow star.
  bool is_starred_;

  GtkThemeProvider* theme_provider_;

  CustomDrawButtonBase unstarred_;
  CustomDrawButtonBase starred_;

  DISALLOW_EVIL_CONSTRUCTORS(ToolbarStarToggleGtk);
};

#endif  // CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_
