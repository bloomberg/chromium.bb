// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

class BubbleGtk;
class GtkThemeService;

namespace gfx {
class Rect;
}

class BubbleDelegateGtk {
 public:
  // Called when the bubble is closing and is about to be deleted.
  // |closed_by_escape| is true if the close is the result of pressing escape.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) = 0;

  // NOTE: The Views interface has CloseOnEscape, except I can't find a place
  // where it ever returns false, so we always allow you to close via escape.

 protected:
  virtual ~BubbleDelegateGtk() {}
};

// This is the GTK implementation of Bubbles. Bubbles are like dialogs, but they
// can point to a given element on the screen. You should call BubbleGtk::Show,
// which will create and display a bubble. The object is self deleting, when the
// bubble is closed, you will be notified via
// BubbleDelegateGtk::BubbleClosing(). Then the widgets and the underlying
// object will be destroyed. You can also close and destroy the bubble by
// calling Close().
class BubbleGtk : public content::NotificationObserver {
 public:
  // The style of the frame of the bubble (includes positioning and arrow).
  enum FrameStyle {
    ANCHOR_TOP_LEFT,
    ANCHOR_TOP_MIDDLE,
    ANCHOR_TOP_RIGHT,
    ANCHOR_BOTTOM_LEFT,
    ANCHOR_BOTTOM_MIDDLE,
    ANCHOR_BOTTOM_RIGHT,
    FLOAT_BELOW_RECT,  // No arrow. Positioned under the supplied rect.
    CENTER_OVER_RECT,  // No arrow. Centered over the supplied rect.
    FIXED_TOP_LEFT,  // No arrow. Shown at top left of |toplevel_window_|.
    FIXED_TOP_RIGHT,  // No arrow. Shown at top right of |toplevel_window_|.
  };

  enum BubbleAttribute {
    NONE = 0,
    MATCH_SYSTEM_THEME = 1 << 0,  // Matches system colors/themes when possible.
    POPUP_WINDOW = 1 << 1,  // Displays as popup instead of top-level window.
    GRAB_INPUT = 1 << 2,  // Causes bubble to grab keyboard/pointer input.
    NO_ACCELERATORS = 1 << 3, // Does not register any of the default bubble
                              // accelerators.
  };

  // Show a bubble, pointing at the area |rect| (in coordinates relative to
  // |anchor_widget|'s origin). A bubble will try to fit on the screen, so it
  // can point to any edge of |rect|. If |rect| is NULL, the widget's entire
  // area will be used. The bubble will host the |content| widget. Its arrow
  // will be drawn according to |frame_style| if possible, and will be
  // automatically flipped in RTL locales. The |delegate| will be notified when
  // the bubble is closed. The bubble will perform an X grab of the pointer and
  // keyboard, and will close itself if a click is received outside of the
  // bubble.
  static BubbleGtk* Show(GtkWidget* anchor_widget,
                         const gfx::Rect* rect,
                         GtkWidget* content,
                         FrameStyle frame_style,
                         int attribute_flags,
                         GtkThemeService* provider,
                         BubbleDelegateGtk* delegate);

  // Close the bubble if it's open.  This will delete the widgets and object,
  // so you shouldn't hold a BubbleGtk pointer after calling Close().
  void Close();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Change an input-grabbing bubble into a non-input-grabbing bubble. This
  // allows a window to change from auto closing when it loses to focus to being
  // a window that does not auto close, and is useful if an auto closing window
  // starts being inspected.
  void StopGrabbingInput();

  GtkWindow* GetNativeWindow();

  GtkWidget* anchor_widget() { return anchor_widget_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleGtkTest, ArrowLocation);
  FRIEND_TEST_ALL_PREFIXES(BubbleGtkTest, NoArrow);

  enum FrameType {
    FRAME_MASK,
    FRAME_STROKE,
  };

  BubbleGtk(GtkThemeService* provider,
            FrameStyle frame_style,
            int attribute_flags);
  virtual ~BubbleGtk();

  // Creates the Bubble.
  void Init(GtkWidget* anchor_widget,
            const gfx::Rect* rect,
            GtkWidget* content,
            int attribute_flags);

  // Make the points for our polygon frame, either for fill (the mask), or for
  // when we stroke the border.
  static std::vector<GdkPoint> MakeFramePolygonPoints(
      FrameStyle frame_style,
      int width,
      int height,
      FrameType type);

  // Get the allowed frame style (which is a function of the preferred style and
  // of the direction that the bubble should be facing to fit onscreen).
  // |arrow_x| (or |arrow_y|) is the X component (or Y component) in screen
  // coordinates of the point at which the bubble's arrow should be aimed,
  // respectively.  |width| (or |height|) is the bubble's width (or height).
  static FrameStyle GetAllowedFrameStyle(FrameStyle preferred_location,
                                         int arrow_x,
                                         int arrow_y,
                                         int width,
                                         int height);

  // Updates the frame style based on the toplevel window's current position and
  // the bubble's size.  If the |force_move_and_reshape| is true or the location
  // changes, moves and reshapes the window and returns true.
  bool UpdateFrameStyle(bool force_move_and_reshape);

  // Reshapes the window and updates |mask_region_|.
  void UpdateWindowShape();

  // Calculate the current screen position for the bubble's window (per
  // |toplevel_window_|'s position as of its most-recent ConfigureNotify event
  // and |rect_|) and move it there.
  void MoveWindow();

  // Restack the bubble's window directly above |toplevel_window_|.
  void StackWindow();

  // Sets the delegate.
  void set_delegate(BubbleDelegateGtk* delegate) { delegate_ = delegate; }

  // Grab (in the X sense) the pointer and keyboard.  This is needed to make
  // sure that we have the input focus.
  void GrabPointerAndKeyboard();

  CHROMEG_CALLBACK_3(BubbleGtk, gboolean, OnGtkAccelerator, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);

  CHROMEGTK_CALLBACK_1(BubbleGtk, gboolean, OnExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(BubbleGtk, void, OnSizeAllocate, GtkAllocation*);
  CHROMEGTK_CALLBACK_1(BubbleGtk, gboolean, OnButtonPress, GdkEventButton*);
  CHROMEGTK_CALLBACK_0(BubbleGtk, gboolean, OnDestroy);
  CHROMEGTK_CALLBACK_0(BubbleGtk, void, OnHide);
  CHROMEGTK_CALLBACK_1(BubbleGtk, gboolean, OnGrabBroken, GdkEventGrabBroken*);
  CHROMEGTK_CALLBACK_0(BubbleGtk, void, OnForeshadowWidgetHide);
  CHROMEGTK_CALLBACK_1(BubbleGtk, gboolean, OnToplevelConfigure,
                       GdkEventConfigure*);
  CHROMEGTK_CALLBACK_1(BubbleGtk, gboolean, OnToplevelUnmap, GdkEvent*);
  CHROMEGTK_CALLBACK_1(BubbleGtk, void, OnAnchorAllocate, GtkAllocation*);
  CHROMEGTK_CALLBACK_0(BubbleGtk, void, OnAnchorDestroy);
  CHROMEGTK_CALLBACK_0(BubbleGtk, void, OnToplevelDestroy);

  // The caller supplied delegate, can be NULL.
  BubbleDelegateGtk* delegate_;

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
  GtkWidget* toplevel_window_;

  // The widget that we use to relatively position the popup window.
  GtkWidget* anchor_widget_;

  // Provides an offset from |anchor_widget_|'s origin for MoveWindow() to
  // use.
  gfx::Rect rect_;

  // The current shape of |window_| (used to test whether clicks fall in it or
  // not).
  GdkRegion* mask_region_;

  // The frame style given to |Show()| that will attempt to be used. It will be
  // flipped in RTL. If there's not enough screen space for the given
  // FrameStyle, this may be changed and differ from |actual_frame_style_|.
  FrameStyle requested_frame_style_;

  // The currently used frame style given screen size and directionality.
  FrameStyle actual_frame_style_;

  // Whether the background should match the system theme, when the system theme
  // is being used. For example, the bookmark bubble does, but extension popups
  // do not.
  bool match_system_theme_;

  // If true, the popup owns all X input for the duration of its existence.
  // This will usually be true, the exception being when inspecting extension
  // popups with dev tools.
  bool grab_input_;

  bool closed_by_escape_;

  content::NotificationRegistrar registrar_;

  ui::GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(BubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_GTK_H_
