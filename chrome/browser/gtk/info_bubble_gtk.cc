// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/info_bubble_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "app/gfx/gtk_util.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"

namespace {

// The height of the arrow, and the width will be about twice the height.
const int kArrowSize = 5;
// Number of pixels to the start of the arrow from the edge of the window.
const int kArrowX = 13;
// Number of pixels between the tip of the arrow and the region we're
// pointing to.
const int kArrowToContentPadding = -6;
// We draw flat diagonal corners, each corner is an NxN square.
const int kCornerSize = 3;
// Margins around the content.
const int kTopMargin = kArrowSize + kCornerSize + 6;
const int kBottomMargin = kCornerSize + 6;
const int kLeftMargin = kCornerSize + 6;
const int kRightMargin = kCornerSize + 6;

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kFrameColor = GDK_COLOR_RGB(0x63, 0x63, 0x63);

enum FrameType {
  FRAME_MASK,
  FRAME_STROKE,
};

// Make the points for our polygon frame, either for fill (the mask), or for
// when we stroke the border.  NOTE: This seems a bit overcomplicated, but it
// requires a bunch of careful fudging to get the pixels rasterized exactly
// where we want them, the arrow to have a 1 pixel point, etc.
// TODO(deanm): Windows draws with Skia and uses some PNG images for the
// corners.  This is a lot more work, but they get anti-aliasing.
std::vector<GdkPoint> MakeFramePolygonPoints(int width,
                                             int height,
                                             FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  std::vector<GdkPoint> points;

  bool ltr = l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT;
  // If we have a stroke, we have to offset some of our points by 1 pixel.
  // We have to inset by 1 pixel when we draw horizontal lines that are on the
  // bottom or when we draw vertical lines that are closer to the end (end is
  // right for ltr).
  int y_off = (type == FRAME_MASK) ? 0 : -1;
  // We use this one for LTR.
  int x_off_l = ltr ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !ltr ? -y_off : 0;

  // Top left corner.
  points.push_back(MakeBidiGdkPoint(
      x_off_r, kArrowSize + kCornerSize - 1, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r - 1, kArrowSize, width, ltr));

  // The arrow.
  points.push_back(MakeBidiGdkPoint(
      kArrowX - kArrowSize + x_off_r, kArrowSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      kArrowX + x_off_r, 0, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      kArrowX + 1 + x_off_l, 0, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      kArrowX + kArrowSize + 1 + x_off_l, kArrowSize, width, ltr));

  // Top right corner.
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + 1 + x_off_l, kArrowSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, kArrowSize + kCornerSize - 1, width, ltr));

  // Bottom right corner.
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, height - kCornerSize, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_r, height + y_off, width, ltr));

  // Bottom left corner.
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_l, height + y_off, width, ltr));
  points.push_back(MakeBidiGdkPoint(
      x_off_r, height - kCornerSize, width, ltr));

  return points;
}

gboolean HandleExpose(GtkWidget* widget,
                      GdkEventExpose* event,
                      gpointer unused) {
  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);
  gdk_gc_set_rgb_fg_color(gc, &kFrameColor);

  // Stroke the frame border.
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      widget->allocation.width, widget->allocation.height, FRAME_STROKE);
  gdk_draw_polygon(drawable, gc, FALSE, &points[0], points.size());

  g_object_unref(gc);
  return FALSE;  // Propagate so our children paint, etc.
}

}  // namespace

// static
InfoBubbleGtk* InfoBubbleGtk::Show(GtkWindow* toplevel_window,
                                   const gfx::Rect& rect,
                                   GtkWidget* content,
                                   GtkThemeProvider* provider,
                                   InfoBubbleGtkDelegate* delegate) {
  InfoBubbleGtk* bubble = new InfoBubbleGtk(provider);
  bubble->Init(toplevel_window, rect, content);
  bubble->set_delegate(delegate);
  return bubble;
}

InfoBubbleGtk::InfoBubbleGtk(GtkThemeProvider* provider)
    : delegate_(NULL),
      window_(NULL),
      theme_provider_(provider),
      accel_group_(gtk_accel_group_new()),
      toplevel_window_(NULL),
      mask_region_(NULL) {
}

InfoBubbleGtk::~InfoBubbleGtk() {
  g_object_unref(accel_group_);
  if (mask_region_) {
    gdk_region_destroy(mask_region_);
    mask_region_ = NULL;
  }

  g_signal_handlers_disconnect_by_func(
      toplevel_window_,
      reinterpret_cast<gpointer>(HandleToplevelConfigureThunk),
      this);
  g_signal_handlers_disconnect_by_func(
      toplevel_window_,
      reinterpret_cast<gpointer>(HandleToplevelUnmapThunk),
      this);
  toplevel_window_ = NULL;
}

void InfoBubbleGtk::Init(GtkWindow* toplevel_window,
                         const gfx::Rect& rect,
                         GtkWidget* content) {
  DCHECK(!window_);
  toplevel_window_ = toplevel_window;
  rect_ = rect;

  window_ = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_set_app_paintable(window_, TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(window_, TRUE);

  // Attach our accelerator group to the window with an escape accelerator.
  gtk_accel_group_connect(accel_group_, GDK_Escape,
      static_cast<GdkModifierType>(0), static_cast<GtkAccelFlags>(0),
      g_cclosure_new(G_CALLBACK(&HandleEscapeThunk), this, NULL));
  gtk_window_add_accel_group(GTK_WINDOW(window_), accel_group_);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
                            kTopMargin, kBottomMargin,
                            kLeftMargin, kRightMargin);

  gtk_container_add(GTK_CONTAINER(alignment), content);
  gtk_container_add(GTK_CONTAINER(window_), alignment);

  // GtkWidget only exposes the bitmap mask interface.  Use GDK to more
  // efficently mask a GdkRegion.  Make sure the window is realized during
  // HandleSizeAllocate, so the mask can be applied to the GdkWindow.
  gtk_widget_realize(window_);

  // For RTL, we will have to move the window again when it is allocated, but
  // this should be somewhat close to its final position.
  MoveWindow();
  GtkRequisition req;
  gtk_widget_size_request(window_, &req);

  StackWindow();

  gtk_widget_add_events(window_, GDK_BUTTON_PRESS_MASK |
                                 GDK_BUTTON_RELEASE_MASK);

  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(HandleExpose), NULL);
  g_signal_connect(window_, "size-allocate",
                   G_CALLBACK(HandleSizeAllocateThunk), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(&HandleButtonPressThunk), this);
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);

  g_signal_connect(toplevel_window, "configure-event",
                   G_CALLBACK(&HandleToplevelConfigureThunk), this);
  g_signal_connect(toplevel_window, "unmap-event",
                   G_CALLBACK(&HandleToplevelUnmapThunk), this);

  gtk_widget_show_all(window_);

  // We add a GTK (application-level) grab.  This means we will get all
  // mouse events for our application, even if they were delivered on another
  // window.  We don't need this to get button presses outside of the bubble's
  // window so we'll know to close it (the pointer grab takes care of that), but
  // it prevents other widgets from getting highlighted when the pointer moves
  // over them.
  //
  // (Ideally we wouldn't add the window to a group and it would just get all
  // the mouse events, but gtk_grab_add() doesn't appear to do anything in that
  // case.  Adding it to the toplevel window's group first appears to block
  // enter/leave events for that window and its subwindows, although other
  // browser windows still receive them).
  gtk_window_group_add_window(gtk_window_get_group(toplevel_window),
                              GTK_WINDOW(window_));
  gtk_grab_add(window_);

  GrabPointerAndKeyboard();

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
}

void InfoBubbleGtk::MoveWindow() {
  gint toplevel_x = 0, toplevel_y = 0;
  gdk_window_get_position(
      GTK_WIDGET(toplevel_window_)->window, &toplevel_x, &toplevel_y);

  gint screen_x = 0;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    screen_x = toplevel_x + rect_.x() + (rect_.width() / 2) -
               window_->allocation.width + kArrowX;
  } else {
    screen_x = toplevel_x + rect_.x() + (rect_.width() / 2) - kArrowX;
  }

  gint screen_y = toplevel_y + rect_.y() + rect_.height() +
                  kArrowToContentPadding;

  gtk_window_move(GTK_WINDOW(window_), screen_x, screen_y);
}

void InfoBubbleGtk::StackWindow() {
  // Stack our window directly above the toplevel window.
  gtk_util::StackPopupWindow(window_, GTK_WIDGET(toplevel_window_));
}

void InfoBubbleGtk::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK_EQ(type.value, NotificationType::BROWSER_THEME_CHANGED);
  if (theme_provider_->UseGtkTheme()) {
    gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, NULL);
  } else {
    // Set the background color, so we don't need to paint it manually.
    gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kBackgroundColor);
  }
}

void InfoBubbleGtk::HandlePointerAndKeyboardUngrabbedByContent() {
  GrabPointerAndKeyboard();
}

void InfoBubbleGtk::CloseInternal(bool closed_by_escape) {
  // Notify the delegate that we're about to close.  This gives the chance
  // to save state / etc from the hosted widget before it's destroyed.
  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape);

  // We don't need to ungrab the pointer or keyboard here; the X server will
  // automatically do that when we destroy our window.

  DCHECK(window_);
  gtk_widget_destroy(window_);
  // |this| has been deleted, see HandleDestroy.
}

void InfoBubbleGtk::GrabPointerAndKeyboard() {
  // Install X pointer and keyboard grabs to make sure that we have the focus
  // and get all mouse and keyboard events until we're closed.
  GdkGrabStatus pointer_grab_status =
      gdk_pointer_grab(window_->window,
                       TRUE,                   // owner_events
                       GDK_BUTTON_PRESS_MASK,  // event_mask
                       NULL,                   // confine_to
                       NULL,                   // cursor
                       GDK_CURRENT_TIME);
  if (pointer_grab_status != GDK_GRAB_SUCCESS) {
    // This will fail if someone else already has the pointer grabbed, but
    // there's not really anything we can do about that.
    DLOG(ERROR) << "Unable to grab pointer (status="
                << pointer_grab_status << ")";
  }
  GdkGrabStatus keyboard_grab_status =
      gdk_keyboard_grab(window_->window,
                        FALSE,  // owner_events
                        GDK_CURRENT_TIME);
  if (keyboard_grab_status != GDK_GRAB_SUCCESS) {
    DLOG(ERROR) << "Unable to grab keyboard (status="
                << keyboard_grab_status << ")";
  }
}

gboolean InfoBubbleGtk::HandleEscape() {
  CloseInternal(true);  // Close by escape.
  return TRUE;
}

// When our size is initially allocated or changed, we need to recompute
// and apply our shape mask region.
void InfoBubbleGtk::HandleSizeAllocate() {
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    MoveWindow();

  DCHECK(window_->allocation.x == 0 && window_->allocation.y == 0);
  if (mask_region_) {
    gdk_region_destroy(mask_region_);
    mask_region_ = NULL;
  }
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      window_->allocation.width, window_->allocation.height, FRAME_MASK);
  mask_region_ = gdk_region_polygon(&points[0],
                                    points.size(),
                                    GDK_EVEN_ODD_RULE);
  gdk_window_shape_combine_region(window_->window, mask_region_, 0, 0);
}

gboolean InfoBubbleGtk::HandleButtonPress(GdkEventButton* event) {
  // If we got a click in our own window, that's okay (we need to additionally
  // check that it falls within our bounds, since we've grabbed the pointer and
  // some events that actually occurred in other windows will be reported with
  // respect to our window).
  if (event->window == window_->window &&
      (mask_region_ && gdk_region_point_in(mask_region_, event->x, event->y))) {
    return FALSE;  // Propagate.
  }

  // Our content widget got a click.
  if (event->window != window_->window &&
      gdk_window_get_toplevel(event->window) == window_->window) {
    return FALSE;
  }

  // Otherwise we had a click outside of our window, close ourself.
  Close();
  return TRUE;
}

gboolean InfoBubbleGtk::HandleDestroy() {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroy the widget manually, or the window was closed via X.  This will
  // delete the InfoBubbleGtk object.
  delete this;
  return FALSE;  // Propagate.
}

gboolean InfoBubbleGtk::HandleToplevelConfigure(GdkEventConfigure* event) {
  MoveWindow();
  StackWindow();
  return FALSE;
}

gboolean InfoBubbleGtk::HandleToplevelUnmap() {
  Close();
  return FALSE;
}
