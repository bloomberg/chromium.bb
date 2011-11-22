// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_STATUS_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_STATUS_BUBBLE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/ui/status_bubble.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/point.h"

class GtkThemeService;
class Profile;

namespace ui {
class SlideAnimation;
}

// GTK implementation of StatusBubble. Unlike Windows, our status bubble
// doesn't have the nice leave-the-window effect since we can't rely on the
// window manager to not try to be "helpful" and center our popups, etc.
// We therefore position it absolutely in a GtkFixed, that we don't own.
class StatusBubbleGtk : public StatusBubble,
                        public content::NotificationObserver,
                        public ui::AnimationDelegate {
 public:
  explicit StatusBubbleGtk(Profile* profile);
  virtual ~StatusBubbleGtk();

  bool flip_horizontally() const { return flip_horizontally_; }
  int y_offset() const { return y_offset_; }

  // StatusBubble implementation.
  virtual void SetStatus(const string16& status) OVERRIDE;
  virtual void SetURL(const GURL& url, const std::string& languages) OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void MouseMoved(const gfx::Point& location,
                          bool left_content) OVERRIDE;

  // ui::AnimationDelegate implementation.
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Called when the download shelf becomes visible or invisible.
  // This is used by to ensure that the status bubble does not obscure
  // the download shelf, when it is visible.
  virtual void UpdateDownloadShelfVisibility(bool visible) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Top of the widget hierarchy for a StatusBubble. This top level widget is
  // guarenteed to have its gtk_widget_name set to "status-bubble" for
  // identification.
  GtkWidget* widget() { return container_.get(); }

 private:
  // Sets the text of the label widget and controls visibility. (As contrasted
  // with setting the current status or URL text, which may be ignored for now).
  void SetStatusTextTo(const std::string& status_utf8);

  // Sets the status text to the current value of |url_|, eliding it as
  // necessary.
  void SetStatusTextToURL();

  // Sets the status bubble's location in the parent GtkFixed, shows the widget
  // and makes sure that the status bubble has the highest z-order.
  void Show();

  // Builds the widgets, containers, etc.
  void InitWidgets();

  // Notification from the window that we should retheme ourself.
  void UserChangedTheme();

  // Sets whether the bubble should be flipped horizontally and displayed on the
  // opposite side of the tab contents.  Reshapes the container and queues a
  // redraw if necessary.
  void SetFlipHorizontally(bool flip_horizontally);

  // Expand the bubble up to the full width of the browser, so that the entire
  // URL may be seen. Called after the user hovers over a link for sufficient
  // time.
  void ExpandURL();

  // Adjust the actual size of the bubble by changing the label's size request.
  void UpdateLabelSizeRequest();

  // Returns true if the status bubble is in the expand-state (i.e., is
  // currently expanded or in the process of expanding).
  bool expanded() {
    return expand_animation_.get();
  }

  CHROMEGTK_CALLBACK_1(StatusBubbleGtk, gboolean, HandleMotionNotify,
                       GdkEventMotion*);

  CHROMEGTK_CALLBACK_1(StatusBubbleGtk, gboolean, HandleEnterNotify,
                       GdkEventCrossing*);

  content::NotificationRegistrar registrar_;

  // Provides colors.
  GtkThemeService* theme_service_;

  // The toplevel event box.
  ui::OwnedWidgetGtk container_;

  // The GtkAlignment holding |label_|.
  GtkWidget* padding_;

  // The GtkLabel holding the text.
  ui::OwnedWidgetGtk label_;

  // The status text we want to display when there are no URLs to display.
  std::string status_text_;

  // The URL we are displaying for.
  GURL url_;

  // The possibly elided url text we want to display.
  std::string url_text_;

  // Used to determine the character set that the user can read (for eliding
  // the url text).
  std::string languages_;

  // A timer that hides our window after a delay.
  base::OneShotTimer<StatusBubbleGtk> hide_timer_;

  // A timer that expands our window after a delay.
  base::OneShotTimer<StatusBubbleGtk> expand_timer_;

  // The animation for resizing the status bubble on long hovers.
  scoped_ptr<ui::SlideAnimation> expand_animation_;

  // The start and end width of the current resize animation.
  int start_width_;
  int desired_width_;

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

  // Shortly after the cursor enters the status bubble, we'll get a message
  // that the cursor left the content area. This lets us ignore that.
  bool ignore_next_left_content_;
};

#endif  // CHROME_BROWSER_UI_GTK_STATUS_BUBBLE_GTK_H_
