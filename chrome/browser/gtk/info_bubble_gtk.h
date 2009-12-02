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
#include "base/gfx/point.h"
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
  // Where should the arrow be placed relative to the bubble?
  enum ArrowLocationGtk {
    // TODO(derat): Support placing arrows on the bottoms of the bubbles.
    ARROW_LOCATION_TOP_LEFT,
    ARROW_LOCATION_TOP_RIGHT,
  };

  // Show an InfoBubble, pointing at the area |rect| (in coordinates relative to
  // |toplevel_window|'s origin).  An info bubble will try to fit on the screen,
  // so it can point to any edge of |rect|.  The bubble will host the |content|
  // widget.  Its arrow will be drawn at |arrow_location| if possible.  The
  // |delegate| will be notified when the bubble is closed.  The bubble will
  // perform an X grab of the pointer and keyboard, and will close itself if a
  // click is received outside of the bubble.
  static InfoBubbleGtk* Show(GtkWindow* toplevel_window,
                             const gfx::Rect& rect,
                             GtkWidget* content,
                             ArrowLocationGtk arrow_location,
                             bool match_system_theme,
                             GtkThemeProvider* provider,
                             InfoBubbleGtkDelegate* delegate);

  // Close the bubble if it's open.  This will delete the widgets and object,
  // so you shouldn't hold a InfoBubbleGtk pointer after calling Close().
  void Close() { CloseInternal(false); }

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

  explicit InfoBubbleGtk(GtkThemeProvider* provider, bool match_system_theme);
  virtual ~InfoBubbleGtk();

  // Creates the InfoBubble.
  void Init(GtkWindow* toplevel_window,
            const gfx::Rect& rect,
            GtkWidget* content,
            ArrowLocationGtk arrow_location);

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

  // Closes the window and notifies the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void CloseInternal(bool closed_by_escape);

  // Grab (in the X sense) the pointer and keyboard.  This is needed to make
  // sure that we have the input focus.
  void GrabPointerAndKeyboard();

  static gboolean HandleEscapeThunk(GtkAccelGroup* group,
                                    GObject* acceleratable,
                                    guint keyval,
                                    GdkModifierType modifier,
                                    gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleEscape();
  }
  gboolean HandleEscape();

  static gboolean HandleExposeThunk(GtkWidget* widget,
                                    GdkEventExpose* event,
                                    gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleExpose();
  }
  gboolean HandleExpose();

  static void HandleSizeAllocateThunk(GtkWidget* widget,
                                      GtkAllocation* allocation,
                                      gpointer user_data) {
    reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleSizeAllocate();
  }
  void HandleSizeAllocate();

  static gboolean HandleButtonPressThunk(GtkWidget* widget,
                                         GdkEventButton* event,
                                         gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->
        HandleButtonPress(event);
  }
  gboolean HandleButtonPress(GdkEventButton* event);

  static gboolean HandleButtonReleaseThunk(GtkWidget* widget,
                                           GdkEventButton* event,
                                           gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->
        HandleButtonRelease(event);
  }
  gboolean HandleButtonRelease(GdkEventButton* event);

  static gboolean HandleDestroyThunk(GtkWidget* widget,
                                     gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleDestroy();
  }
  gboolean HandleDestroy();

  static gboolean HandleToplevelConfigureThunk(GtkWidget* widget,
                                               GdkEventConfigure* event,
                                               gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->
        HandleToplevelConfigure(event);
  }
  gboolean HandleToplevelConfigure(GdkEventConfigure* event);

  static gboolean HandleToplevelUnmapThunk(GtkWidget* widget,
                                           GdkEvent* event,
                                           gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleToplevelUnmap();
  }
  gboolean HandleToplevelUnmap();

  // The caller supplied delegate, can be NULL.
  InfoBubbleGtkDelegate* delegate_;

  // Our GtkWindow popup window, we don't technically "own" the widget, since
  // it deletes us when it is destroyed.
  GtkWidget* window_;

  // Provides colors and stuff.
  GtkThemeProvider* theme_provider_;

  // The accel group attached to |window_|, to handle closing with escape.
  GtkAccelGroup* accel_group_;

  // The window for which we're being shown (and to which |rect_| is relative).
  GtkWindow* toplevel_window_;

  // Provides an offset from |toplevel_window_|'s origin for MoveWindow() to
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

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleGtk);
};

#endif  // CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_
