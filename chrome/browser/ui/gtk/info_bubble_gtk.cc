// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/info_bubble_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/info_bubble_accelerators_gtk.h"
#include "content/common/notification_service.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"

namespace {

// The height of the arrow, and the width will be about twice the height.
const int kArrowSize = 8;

// Number of pixels to the middle of the arrow from the close edge of the
// window.
const int kArrowX = 18;

// Number of pixels between the tip of the arrow and the region we're
// pointing to.
const int kArrowToContentPadding = -4;

// We draw flat diagonal corners, each corner is an NxN square.
const int kCornerSize = 3;

// Margins around the content.
const int kTopMargin = kArrowSize + kCornerSize - 1;
const int kBottomMargin = kCornerSize - 1;
const int kLeftMargin = kCornerSize - 1;
const int kRightMargin = kCornerSize - 1;

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kFrameColor = GDK_COLOR_RGB(0x63, 0x63, 0x63);

}  // namespace

// static
InfoBubbleGtk* InfoBubbleGtk::Show(GtkWidget* anchor_widget,
                                   const gfx::Rect* rect,
                                   GtkWidget* content,
                                   ArrowLocationGtk arrow_location,
                                   bool match_system_theme,
                                   bool grab_input,
                                   GtkThemeProvider* provider,
                                   InfoBubbleGtkDelegate* delegate) {
  InfoBubbleGtk* bubble = new InfoBubbleGtk(provider, match_system_theme);
  bubble->Init(anchor_widget, rect, content, arrow_location, grab_input);
  bubble->set_delegate(delegate);
  return bubble;
}

InfoBubbleGtk::InfoBubbleGtk(GtkThemeProvider* provider,
                             bool match_system_theme)
    : delegate_(NULL),
      window_(NULL),
      theme_provider_(provider),
      accel_group_(gtk_accel_group_new()),
      toplevel_window_(NULL),
      anchor_widget_(NULL),
      mask_region_(NULL),
      preferred_arrow_location_(ARROW_LOCATION_TOP_LEFT),
      current_arrow_location_(ARROW_LOCATION_TOP_LEFT),
      match_system_theme_(match_system_theme),
      grab_input_(true),
      closed_by_escape_(false) {
}

InfoBubbleGtk::~InfoBubbleGtk() {
  // Notify the delegate that we're about to close.  This gives the chance
  // to save state / etc from the hosted widget before it's destroyed.
  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape_);

  g_object_unref(accel_group_);
  if (mask_region_)
    gdk_region_destroy(mask_region_);
}

void InfoBubbleGtk::Init(GtkWidget* anchor_widget,
                         const gfx::Rect* rect,
                         GtkWidget* content,
                         ArrowLocationGtk arrow_location,
                         bool grab_input) {
  // If there is a current grab widget (menu, other info bubble, etc.), hide it.
  GtkWidget* current_grab_widget = gtk_grab_get_current();
  if (current_grab_widget)
    gtk_widget_hide(current_grab_widget);

  DCHECK(!window_);
  anchor_widget_ = anchor_widget;
  toplevel_window_ = GTK_WINDOW(gtk_widget_get_toplevel(anchor_widget_));
  DCHECK(GTK_WIDGET_TOPLEVEL(toplevel_window_));
  rect_ = rect ? *rect : gtk_util::WidgetBounds(anchor_widget);
  preferred_arrow_location_ = arrow_location;

  grab_input_ = grab_input;
  // Using a TOPLEVEL window may cause placement issues with certain WMs but it
  // is necessary to be able to focus the window.
  window_ = gtk_window_new(grab_input ? GTK_WINDOW_POPUP : GTK_WINDOW_TOPLEVEL);

  gtk_widget_set_app_paintable(window_, TRUE);
  // Resizing is handled by the program, not user.
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);

  // Attach all of the accelerators to the bubble.
  InfoBubbleAcceleratorGtkList acceleratorList =
      InfoBubbleAcceleratorsGtk::GetList();
  for (InfoBubbleAcceleratorGtkList::const_iterator iter =
           acceleratorList.begin();
       iter != acceleratorList.end();
       ++iter) {
    gtk_accel_group_connect(accel_group_,
                            iter->keyval,
                            iter->modifier_type,
                            GtkAccelFlags(0),
                            g_cclosure_new(G_CALLBACK(&OnGtkAcceleratorThunk),
                                           this,
                                           NULL));
  }

  gtk_window_add_accel_group(GTK_WINDOW(window_), accel_group_);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
                            kTopMargin, kBottomMargin,
                            kLeftMargin, kRightMargin);

  gtk_container_add(GTK_CONTAINER(alignment), content);
  gtk_container_add(GTK_CONTAINER(window_), alignment);

  // GtkWidget only exposes the bitmap mask interface.  Use GDK to more
  // efficently mask a GdkRegion.  Make sure the window is realized during
  // OnSizeAllocate, so the mask can be applied to the GdkWindow.
  gtk_widget_realize(window_);

  UpdateArrowLocation(true);  // Force move and reshape.
  StackWindow();

  gtk_widget_add_events(window_, GDK_BUTTON_PRESS_MASK);

  signals_.Connect(window_, "expose-event", G_CALLBACK(OnExposeThunk), this);
  signals_.Connect(window_, "size-allocate", G_CALLBACK(OnSizeAllocateThunk),
                   this);
  signals_.Connect(window_, "button-press-event",
                   G_CALLBACK(OnButtonPressThunk), this);
  signals_.Connect(window_, "destroy", G_CALLBACK(OnDestroyThunk), this);
  signals_.Connect(window_, "hide", G_CALLBACK(OnHideThunk), this);

  // If the toplevel window is being used as the anchor, then the signals below
  // are enough to keep us positioned correctly.
  if (anchor_widget_ != GTK_WIDGET(toplevel_window_)) {
    signals_.Connect(anchor_widget_, "size-allocate",
                     G_CALLBACK(OnAnchorAllocateThunk), this);
    signals_.Connect(anchor_widget_, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &anchor_widget_);
  }

  signals_.Connect(toplevel_window_, "configure-event",
                   G_CALLBACK(OnToplevelConfigureThunk), this);
  signals_.Connect(toplevel_window_, "unmap-event",
                   G_CALLBACK(OnToplevelUnmapThunk), this);
  // Set |toplevel_window_| to NULL if it gets destroyed.
  signals_.Connect(toplevel_window_, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &toplevel_window_);

  gtk_widget_show_all(window_);

  if (grab_input_) {
    gtk_grab_add(window_);
    GrabPointerAndKeyboard();
  }

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
}

// NOTE: This seems a bit overcomplicated, but it requires a bunch of careful
// fudging to get the pixels rasterized exactly where we want them, the arrow to
// have a 1 pixel point, etc.
// TODO(deanm): Windows draws with Skia and uses some PNG images for the
// corners.  This is a lot more work, but they get anti-aliasing.
// static
std::vector<GdkPoint> InfoBubbleGtk::MakeFramePolygonPoints(
    ArrowLocationGtk arrow_location,
    int width,
    int height,
    FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  std::vector<GdkPoint> points;

  bool on_left = (arrow_location == ARROW_LOCATION_TOP_LEFT);

  // If we're stroking the frame, we need to offset some of our points by 1
  // pixel.  We do this when we draw horizontal lines that are on the bottom or
  // when we draw vertical lines that are closer to the end (where "end" is the
  // right side for ARROW_LOCATION_TOP_LEFT).
  int y_off = (type == FRAME_MASK) ? 0 : -1;
  // We use this one for arrows located on the left.
  int x_off_l = on_left ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !on_left ? -y_off : 0;

  // Top left corner.
  points.push_back(MakeBidiGdkPoint(
      x_off_r, kArrowSize + kCornerSize - 1, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r - 1, kArrowSize, width, on_left));

  // The arrow.
  points.push_back(MakeBidiGdkPoint(
      kArrowX - kArrowSize + x_off_r, kArrowSize, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      kArrowX + x_off_r, 0, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      kArrowX + 1 + x_off_l, 0, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      kArrowX + kArrowSize + 1 + x_off_l, kArrowSize, width, on_left));

  // Top right corner.
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + 1 + x_off_l, kArrowSize, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, kArrowSize + kCornerSize - 1, width, on_left));

  // Bottom right corner.
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, height - kCornerSize, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_r, height + y_off, width, on_left));

  // Bottom left corner.
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_l, height + y_off, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      x_off_r, height - kCornerSize, width, on_left));

  return points;
}

InfoBubbleGtk::ArrowLocationGtk InfoBubbleGtk::GetArrowLocation(
    ArrowLocationGtk preferred_location, int arrow_x, int width) {
  bool wants_left = (preferred_location == ARROW_LOCATION_TOP_LEFT);
  int screen_width = gdk_screen_get_width(gdk_screen_get_default());

  bool left_is_onscreen = (arrow_x - kArrowX + width < screen_width);
  bool right_is_onscreen = (arrow_x + kArrowX - width >= 0);

  // Use the requested location if it fits onscreen, use whatever fits
  // otherwise, and use the requested location if neither fits.
  if (left_is_onscreen && (wants_left || !right_is_onscreen))
    return ARROW_LOCATION_TOP_LEFT;
  if (right_is_onscreen && (!wants_left || !left_is_onscreen))
    return ARROW_LOCATION_TOP_RIGHT;
  return (wants_left ? ARROW_LOCATION_TOP_LEFT : ARROW_LOCATION_TOP_RIGHT);
}

bool InfoBubbleGtk::UpdateArrowLocation(bool force_move_and_reshape) {
  if (!toplevel_window_ || !anchor_widget_)
    return false;

  gint toplevel_x = 0, toplevel_y = 0;
  gdk_window_get_position(
      GTK_WIDGET(toplevel_window_)->window, &toplevel_x, &toplevel_y);
  int offset_x, offset_y;
  gtk_widget_translate_coordinates(anchor_widget_, GTK_WIDGET(toplevel_window_),
                                   rect_.x(), rect_.y(), &offset_x, &offset_y);

  ArrowLocationGtk old_location = current_arrow_location_;
  current_arrow_location_ = GetArrowLocation(
      preferred_arrow_location_,
      toplevel_x + offset_x + (rect_.width() / 2),  // arrow_x
      window_->allocation.width);

  if (force_move_and_reshape || current_arrow_location_ != old_location) {
    UpdateWindowShape();
    MoveWindow();
    // We need to redraw the entire window to repaint its border.
    gtk_widget_queue_draw(window_);
    return true;
  }
  return false;
}

void InfoBubbleGtk::UpdateWindowShape() {
  if (mask_region_) {
    gdk_region_destroy(mask_region_);
    mask_region_ = NULL;
  }
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      current_arrow_location_,
      window_->allocation.width, window_->allocation.height,
      FRAME_MASK);
  mask_region_ = gdk_region_polygon(&points[0],
                                    points.size(),
                                    GDK_EVEN_ODD_RULE);
  gdk_window_shape_combine_region(window_->window, NULL, 0, 0);
  gdk_window_shape_combine_region(window_->window, mask_region_, 0, 0);
}

void InfoBubbleGtk::MoveWindow() {
  if (!toplevel_window_ || !anchor_widget_)
    return;

  gint toplevel_x = 0, toplevel_y = 0;
  gdk_window_get_position(
      GTK_WIDGET(toplevel_window_)->window, &toplevel_x, &toplevel_y);

  int offset_x, offset_y;
  gtk_widget_translate_coordinates(anchor_widget_, GTK_WIDGET(toplevel_window_),
                                   rect_.x(), rect_.y(), &offset_x, &offset_y);

  gint screen_x = 0;
  if (current_arrow_location_ == ARROW_LOCATION_TOP_LEFT) {
    screen_x = toplevel_x + offset_x + (rect_.width() / 2) - kArrowX;
  } else if (current_arrow_location_ == ARROW_LOCATION_TOP_RIGHT) {
    screen_x = toplevel_x + offset_x + (rect_.width() / 2) -
               window_->allocation.width + kArrowX;
  } else {
    NOTREACHED();
  }

  gint screen_y = toplevel_y + offset_y + rect_.height() +
                  kArrowToContentPadding;

  gtk_window_move(GTK_WINDOW(window_), screen_x, screen_y);
}

void InfoBubbleGtk::StackWindow() {
  // Stack our window directly above the toplevel window.
  if (toplevel_window_)
    gtk_util::StackPopupWindow(window_, GTK_WIDGET(toplevel_window_));
}

void InfoBubbleGtk::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK_EQ(type.value, NotificationType::BROWSER_THEME_CHANGED);
  if (theme_provider_->UseGtkTheme() && match_system_theme_) {
    gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, NULL);
  } else {
    // Set the background color, so we don't need to paint it manually.
    gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kBackgroundColor);
  }
}

void InfoBubbleGtk::HandlePointerAndKeyboardUngrabbedByContent() {
  if (grab_input_)
    GrabPointerAndKeyboard();
}

void InfoBubbleGtk::Close() {
  // We don't need to ungrab the pointer or keyboard here; the X server will
  // automatically do that when we destroy our window.
  DCHECK(window_);
  gtk_widget_destroy(window_);
  // |this| has been deleted, see OnDestroy.
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

gboolean InfoBubbleGtk::OnGtkAccelerator(GtkAccelGroup* group,
                                         GObject* acceleratable,
                                         guint keyval,
                                         GdkModifierType modifier) {
  GdkEventKey msg;
  GdkKeymapKey* keys;
  gint n_keys;

  switch (keyval) {
    case GDK_Escape:
      // Close on Esc and trap the accelerator
      closed_by_escape_ = true;
      Close();
      return TRUE;
    case GDK_w:
      // Close on C-w and forward the accelerator
      if (modifier & GDK_CONTROL_MASK) {
        Close();
      }
      break;
    default:
      return FALSE;
  }

  gdk_keymap_get_entries_for_keyval(NULL,
                                    keyval,
                                    &keys,
                                    &n_keys);
  if (n_keys) {
    // Forward the accelerator to root window the bubble is anchored
    // to for further processing
    msg.type = GDK_KEY_PRESS;
    msg.window = GTK_WIDGET(toplevel_window_)->window;
    msg.send_event = TRUE;
    msg.time = GDK_CURRENT_TIME;
    msg.state = modifier | GDK_MOD2_MASK;
    msg.keyval = keyval;
    // length and string are deprecated and thus zeroed out
    msg.length = 0;
    msg.string = NULL;
    msg.hardware_keycode = keys[0].keycode;
    msg.group = keys[0].group;
    msg.is_modifier = 0;

    g_free(keys);

    gtk_main_do_event(reinterpret_cast<GdkEvent*>(&msg));
  } else {
    // This means that there isn't a h/w code for the keyval in the
    // current keymap, which is weird but possible if the keymap just
    // changed. This isn't a critical error, but might be indicative
    // of something off if it happens regularly.
    DLOG(WARNING) << "Found no keys for value " << keyval;
  }
  return TRUE;
}

gboolean InfoBubbleGtk::OnExpose(GtkWidget* widget, GdkEventExpose* expose) {
  GdkDrawable* drawable = GDK_DRAWABLE(window_->window);
  GdkGC* gc = gdk_gc_new(drawable);
  gdk_gc_set_rgb_fg_color(gc, &kFrameColor);

  // Stroke the frame border.
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      current_arrow_location_,
      window_->allocation.width, window_->allocation.height,
      FRAME_STROKE);
  gdk_draw_polygon(drawable, gc, FALSE, &points[0], points.size());

  g_object_unref(gc);
  return FALSE;  // Propagate so our children paint, etc.
}

// When our size is initially allocated or changed, we need to recompute
// and apply our shape mask region.
void InfoBubbleGtk::OnSizeAllocate(GtkWidget* widget,
                                   GtkAllocation* allocation) {
  if (!UpdateArrowLocation(false)) {
    UpdateWindowShape();
    if (current_arrow_location_ == ARROW_LOCATION_TOP_RIGHT)
      MoveWindow();
  }
}

gboolean InfoBubbleGtk::OnButtonPress(GtkWidget* widget,
                                      GdkEventButton* event) {
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

  if (grab_input_) {
    // Otherwise we had a click outside of our window, close ourself.
    Close();
    return TRUE;
  }

  return FALSE;
}

gboolean InfoBubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroy the widget manually, or the window was closed via X.  This will
  // delete the InfoBubbleGtk object.
  delete this;
  return FALSE;  // Propagate.
}

void InfoBubbleGtk::OnHide(GtkWidget* widget) {
  gtk_widget_destroy(widget);
}

gboolean InfoBubbleGtk::OnToplevelConfigure(GtkWidget* widget,
                                            GdkEventConfigure* event) {
  if (!UpdateArrowLocation(false))
    MoveWindow();
  StackWindow();
  return FALSE;
}

gboolean InfoBubbleGtk::OnToplevelUnmap(GtkWidget* widget, GdkEvent* event) {
  Close();
  return FALSE;
}

void InfoBubbleGtk::OnAnchorAllocate(GtkWidget* widget,
                                     GtkAllocation* allocation) {
  if (!UpdateArrowLocation(false))
    MoveWindow();
}
