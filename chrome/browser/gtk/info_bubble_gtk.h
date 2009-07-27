// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of InfoBubbles.  InfoBubbles are like
// dialogs, but they point to a given element on the screen.  You should call
// InfoBubbleGtk::Show, which will create and display a bubble.  The object is
// self deleting, when the bubble is closed, you will be notified via
// InfoBubbleGtkDelegate::InfoBubbleClosing().  Then the widgets and the
// underlying object will be destroyed.  You can also close and destroy the
// bubble by calling Close().

#ifndef CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/common/notification_registrar.h"

class GtkThemeProvider;
class InfoBubbleGtk;
namespace gfx {
class Rect;
}

class InfoBubbleGtkDelegate {
 public:
  // Called when the InfoBubble is closing and is about to be deleted.
  // |closed_by_escape| is true if the close is the result of pressing escape.
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape) = 0;

  // NOTE: The Views interface has CloseOnEscape, except I can't find a place
  // where it ever returns false, so we always allow you to close via escape.
};

class InfoBubbleGtk : public NotificationObserver {
 public:
  // Show an InfoBubble, pointing at the area |rect| (in screen coordinates).
  // An infobubble will try to fit on the screen, so it can point to any edge
  // of |rect|.  The bubble will host the |content| widget.  The |delegate|
  // will be notified when things like closing are happening.
  static InfoBubbleGtk* Show(GtkWindow* transient_toplevel,
                             const gfx::Rect& rect,
                             GtkWidget* content,
                             GtkThemeProvider* provider,
                             InfoBubbleGtkDelegate* delegate);

  // Close the bubble if it's open.  This will delete the widgets and object,
  // so you shouldn't hold a InfoBubbleGtk pointer after calling Close().
  void Close() { Close(false); }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // This returns the toplevel GtkWindow that is the transient parent of
  // |bubble_window|, or NULL if |bubble_window| isn't the GdkWindow
  // for an InfoBubbleGtk.
  static GtkWindow* GetToplevelForInfoBubble(const GdkWindow* bubble_window);

 private:
  explicit InfoBubbleGtk(GtkThemeProvider* provider);
  virtual ~InfoBubbleGtk();

  // Creates the InfoBubble.
  void Init(GtkWindow* transient_toplevel,
            const gfx::Rect& rect,
            GtkWidget* content);

  // Sets |screen_x_| according to our allocation and |rect_|.
  void UpdateScreenX();

  // Sets the delegate.
  void set_delegate(InfoBubbleGtkDelegate* delegate) { delegate_ = delegate; }

  // Closes the window and notifies the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void Close(bool closed_by_escape);

  static gboolean HandleEscapeThunk(GtkAccelGroup* group,
                                    GObject* acceleratable,
                                    guint keyval,
                                    GdkModifierType modifier,
                                    gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleEscape();
  }
  gboolean HandleEscape();

  static void HandleSizeAllocateThunk(GtkWidget* widget,
                                      GtkAllocation* allocation,
                                      gpointer user_data) {
    reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleSizeAllocate();
  }
  void HandleSizeAllocate();

  static gboolean HandleConfigureThunk(GtkWidget* widget,
                                       GdkEventConfigure* event,
                                       gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleConfigure(event);
  }
  gboolean HandleConfigure(GdkEventConfigure* event);

  static gboolean HandleButtonPressThunk(GtkWidget* widget,
                                         GdkEventButton* event,
                                         gpointer userdata) {
    return reinterpret_cast<InfoBubbleGtk*>(userdata)->
        HandleButtonPress(event);
  }
  gboolean HandleButtonPress(GdkEventButton* event);

  static gboolean HandleButtonReleaseThunk(GtkWidget* widget,
                                           GdkEventButton* event,
                                           gpointer userdata) {
    return reinterpret_cast<InfoBubbleGtk*>(userdata)->
        HandleButtonRelease(event);
  }
  gboolean HandleButtonRelease(GdkEventButton* event);

  static gboolean HandleDestroyThunk(GtkWidget* widget,
                                     gpointer userdata) {
    return reinterpret_cast<InfoBubbleGtk*>(userdata)->
        HandleDestroy();
  }
  gboolean HandleDestroy();

  // The caller supplied delegate, can be NULL.
  InfoBubbleGtkDelegate* delegate_;

  // Our GtkWindow popup window, we don't technically "own" the widget, since
  // it deletes us when it is destroyed.
  GtkWidget* window_;

  // Provides colors and stuff.
  GtkThemeProvider* theme_provider_;

  // The accel group attached to |window_|, to handle closing with escape.
  GtkAccelGroup* accel_group_;

  // The rectangle that we use to calculate |screen_x_| and |screen_y_|.
  gfx::Rect rect_;

  // Where we want our window to be positioned on the screen.
  int screen_x_;
  int screen_y_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleGtk);
};

#endif  // CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_
