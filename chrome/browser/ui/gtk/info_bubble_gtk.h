// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of InfoBubbles.  InfoBubbles are like
// dialogs, but they point to a given element on the screen.  You should call
// InfoBubbleGtk::Show, which will create and display a bubble.  The object is
// self deleting, when the bubble is closed, you will be notified via
// InfoBubbleGtkDelegate::InfoBubbleClosing().  Then the widgets and the
// underlying object will be destroyed.  You can also close and destroy the
// bubble by calling Close().

#ifndef CHROME_BROWSER_UI_GTK_INFO_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFO_BUBBLE_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <vector>

#include "base/basictypes.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

class GtkThemeService;
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

 protected:
  virtual ~InfoBubbleGtkDelegate() {}
};

class InfoBubbleGtk : public NotificationObserver {
 public:
  // Where should the arrow be placed relative to the bubble?
  enum ArrowLocationGtk {
    // TODO(derat): Support placing arrows on the bottoms of the bubbles.
    ARROW_LOCATION_TOP_LEFT,
    ARROW_LOCATION_TOP_RIGHT,
  };

  // Show an InfoBubble, pointing at the area |rect| (in coordinates relative to
  // |anchor_widget|'s origin).  An info bubble will try to fit on the screen,
  // so it can point to any edge of |rect|.  If |rect| is NULL, the widget's
  // entire area will be used. The bubble will host the |content|
  // widget.  Its arrow will be drawn at |arrow_location| if possible.  The
  // |delegate| will be notified when the bubble is closed.  The bubble will
  // perform an X grab of the pointer and keyboard, and will close itself if a
  // click is received outside of the bubble.
  static InfoBubbleGtk* Show(GtkWidget* anchor_widget,
                             const gfx::Rect* rect,
                             GtkWidget* content,
                             ArrowLocationGtk arrow_location,
                             bool match_system_theme,
                             bool grab_input,
                             GtkThemeService* provider,
                             InfoBubbleGtkDelegate* delegate);

  // Close the bubble if it's open.  This will delete the widgets and object,
  // so you shouldn't hold a InfoBubbleGtk pointer after calling Close().
  void Close();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // If the content contains widgets that can steal our pointer and keyboard
  // grabs (e.g. GtkComboBox), this method should be called after a widget
  // releases the grabs so we can reacquire them.  Note that this causes a race
  // condition; another client could grab them before we do (ideally, GDK would
  // transfer the grabs back to us when the widget releases them).  The window
  // is small, though, and the worst-case scenario for this seems to just be
  // that the content's widgets will appear inactive even after the user clicks
  // in them.
  void HandlePointerAndKeyboardUngrabbedByContent();

 private:
  enum FrameType {
    FRAME_MASK,
    FRAME_STROKE,
  };

  explicit InfoBubbleGtk(GtkThemeService* provider, bool match_system_theme);
  virtual ~InfoBubbleGtk();

  // Creates the InfoBubble.
  void Init(GtkWidget* anchor_widget,
            const gfx::Rect* rect,
            GtkWidget* content,
            ArrowLocationGtk arrow_location,
            bool grab_input);

  // Make the points for our polygon frame, either for fill (the mask), or for
  // when we stroke the border.
  static std::vector<GdkPoint> MakeFramePolygonPoints(
      ArrowLocationGtk arrow_location,
      int width,
      int height,
      FrameType type);

  // Get the location where the arrow should be placed (which is a function of
  // the preferred location and of the direction that the bubble should be
  // facing to fit onscreen).  |arrow_x| is the X component in screen
  // coordinates of the point at which the bubble's arrow should be aimed, and
  // |width| is the bubble's width.
  static ArrowLocationGtk GetArrowLocation(
      ArrowLocationGtk preferred_location, int arrow_x, int width);

  // Updates |arrow_location_| based on the toplevel window's current position
  // and the bubble's size.  If the |force_move_and_reshape| is true or the
  // location changes, moves and reshapes the window and returns true.
  bool UpdateArrowLocation(bool force_move_and_reshape);

  // Reshapes the window and updates |mask_region_|.
  void UpdateWindowShape();

  // Calculate the current screen position for the bubble's window (per
  // |toplevel_window_|'s position as of its most-recent ConfigureNotify event
  // and |rect_|) and move it there.
  void MoveWindow();

  // Restack the bubble's window directly above |toplevel_window_|.
  void StackWindow();

  // Sets the delegate.
  void set_delegate(InfoBubbleGtkDelegate* delegate) { delegate_ = delegate; }

  // Grab (in the X sense) the pointer and keyboard.  This is needed to make
  // sure that we have the input focus.
  void GrabPointerAndKeyboard();

  CHROMEG_CALLBACK_3(InfoBubbleGtk, gboolean, OnGtkAccelerator, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);

  CHROMEGTK_CALLBACK_1(InfoBubbleGtk, gboolean, OnExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(InfoBubbleGtk, void, OnSizeAllocate, GtkAllocation*);
  CHROMEGTK_CALLBACK_1(InfoBubbleGtk, gboolean, OnButtonPress, GdkEventButton*);
  CHROMEGTK_CALLBACK_0(InfoBubbleGtk, gboolean, OnDestroy);
  CHROMEGTK_CALLBACK_0(InfoBubbleGtk, void, OnHide);
  CHROMEGTK_CALLBACK_1(InfoBubbleGtk, gboolean, OnToplevelConfigure,
                       GdkEventConfigure*);
  CHROMEGTK_CALLBACK_1(InfoBubbleGtk, gboolean, OnToplevelUnmap, GdkEvent*);
  CHROMEGTK_CALLBACK_1(InfoBubbleGtk, void, OnAnchorAllocate, GtkAllocation*);

  // The caller supplied delegate, can be NULL.
  InfoBubbleGtkDelegate* delegate_;

  // Our GtkWindow popup window, we don't technically "own" the widget, since
  // it deletes us when it is destroyed.
  GtkWidget* window_;

  // Provides colors and stuff.
  GtkThemeService* theme_service_;

  // The accel group attached to |window_|, to handle closing with escape.
  GtkAccelGroup* accel_group_;

  // The window for which we're being shown (and to which |rect_| is relative).
  // Note that it's possible for |toplevel_window_| to be NULL if the
  // window is destroyed before this object is destroyed, so it's important
  // to check for that case.
  GtkWindow* toplevel_window_;

  // The widget that we use to relatively position the popup window.
  GtkWidget* anchor_widget_;

  // Provides an offset from |anchor_widget_|'s origin for MoveWindow() to
  // use.
  gfx::Rect rect_;

  // The current shape of |window_| (used to test whether clicks fall in it or
  // not).
  GdkRegion* mask_region_;

  // Where would we prefer for the arrow be drawn relative to the bubble, and
  // where is it currently drawn?
  ArrowLocationGtk preferred_arrow_location_;
  ArrowLocationGtk current_arrow_location_;

  // Whether the background should match the system theme, when the system theme
  // is being used. For example, the bookmark bubble does, but extension popups
  // do not.
  bool match_system_theme_;

  // If true, the popup owns all X input for the duration of its existence.
  // This will usually be true, the exception being when inspecting extension
  // popups with dev tools.
  bool grab_input_;

  bool closed_by_escape_;

  NotificationRegistrar registrar_;

  ui::GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFO_BUBBLE_GTK_H_
