// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/gfx/point.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/status_bubble.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

class GtkThemeProvider;
class GURL;
class Profile;

// GTK implementation of StatusBubble. Unlike Windows, our status bubble
// doesn't have the nice leave-the-window effect since we can't rely on the
// window manager to not try to be "helpful" and center our popups, etc.
// We therefore position it absolutely in a GtkFixed, that we don't own.
class StatusBubbleGtk : public StatusBubble,
                        public NotificationObserver {
 public:
  explicit StatusBubbleGtk(Profile* profile);
  virtual ~StatusBubbleGtk();

  bool flip_horizontally() const { return flip_horizontally_; }
  int y_offset() const { return y_offset_; }

  // StatusBubble implementation.
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved(const gfx::Point& location, bool left_content);

  // Called when the download shelf becomes visible or invisible.
  // This is used by to ensure that the status bubble does not obscure
  // the download shelf, when it is visible.
  virtual void UpdateDownloadShelfVisibility(bool visible);

  // Overridden from NotificationObserver:
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Top of the widget hierarchy for a StatusBubble. This top level widget is
  // guarenteed to have its gtk_widget_name set to "status-bubble" for
  // identification.
  GtkWidget* widget() { return container_.get(); }

 private:
  // Sets the text of the label widget and controls visibility. (As contrasted
  // with setting the current status or URL text, which may be ignored for now).
  void SetStatusTextTo(const std::string& status_utf8);

  // Sets the status bubble's location in the parent GtkFixed, shows the widget
  // and makes sure that the status bubble has the highest z-order.
  void Show();

  // Sets an internal timer to hide the status bubble after a delay.
  void HideInASecond();

  // Builds the widgets, containers, etc.
  void InitWidgets();

  // Notification from the window that we should retheme ourself.
  void UserChangedTheme();

  // Sets whether the bubble should be flipped horizontally and displayed on the
  // opposite side of the tab contents.  Reshapes the container and queues a
  // redraw if necessary.
  void SetFlipHorizontally(bool flip_horizontally);

  static gboolean HandleMotionNotifyThunk(GtkWidget* widget,
                                          GdkEventMotion* event,
                                          gpointer user_data) {
    return reinterpret_cast<StatusBubbleGtk*>(user_data)->
        HandleMotionNotify(event);
  }
  gboolean HandleMotionNotify(GdkEventMotion* event);

  NotificationRegistrar registrar_;

  // Provides colors.
  GtkThemeProvider* theme_provider_;

  // The toplevel event box.
  OwnedWidgetGtk container_;

  // The GtkAlignment holding |label_|.
  GtkWidget* padding_;

  // The GtkLabel holding the text.
  GtkWidget* label_;

  // The status text we want to display when there are no URLs to display.
  std::string status_text_;

  // The url we want to display when there is no status text to display.
  std::string url_text_;

  // A timer that hides our window after a delay.
  ScopedRunnableMethodFactory<StatusBubbleGtk> timer_factory_;

  // Should the bubble be flipped horizontally (e.g. displayed on the right for
  // an LTR language)?  We move the bubble to the other side of the tab contents
  // rather than sliding it down when the download shelf is visible.
  bool flip_horizontally_;

  // Vertical offset used to hide the status bubble as the pointer nears it.
  int y_offset_;

  // If the download shelf is visible, do not obscure it.
  bool download_shelf_is_visible_;

  // 'location' and 'left_content' values from the last invocation of
  // MouseMoved().  We hang onto these so we can move the bubble if necessary
  // when its text changes, triggering a size change.
  gfx::Point last_mouse_location_;
  bool last_mouse_left_content_;
};

#endif  // CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_
