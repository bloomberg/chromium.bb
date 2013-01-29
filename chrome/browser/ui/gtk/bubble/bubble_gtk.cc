// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/gtk/bubble/bubble_accelerators_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_windowing.h"
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

// The amount of padding (in pixels) from the top of |toplevel_window_| to the
// top of |window_| when fixed positioning is used.
const int kFixedPositionPaddingEnd = 10;
const int kFixedPositionPaddingTop = 5;

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kFrameColor = GDK_COLOR_RGB(0x63, 0x63, 0x63);

// Helper functions that encapsulate arrow locations.
bool HasArrow(BubbleGtk::FrameStyle frame_style) {
  return frame_style != BubbleGtk::FLOAT_BELOW_RECT &&
         frame_style != BubbleGtk::CENTER_OVER_RECT &&
         frame_style != BubbleGtk::FIXED_TOP_LEFT &&
         frame_style != BubbleGtk::FIXED_TOP_RIGHT;
}

bool IsArrowLeft(BubbleGtk::FrameStyle frame_style) {
  return frame_style == BubbleGtk::ANCHOR_TOP_LEFT ||
         frame_style == BubbleGtk::ANCHOR_BOTTOM_LEFT;
}

bool IsArrowMiddle(BubbleGtk::FrameStyle frame_style) {
  return frame_style == BubbleGtk::ANCHOR_TOP_MIDDLE ||
         frame_style == BubbleGtk::ANCHOR_BOTTOM_MIDDLE;
}

bool IsArrowRight(BubbleGtk::FrameStyle frame_style) {
  return frame_style == BubbleGtk::ANCHOR_TOP_RIGHT ||
         frame_style == BubbleGtk::ANCHOR_BOTTOM_RIGHT;
}

bool IsArrowTop(BubbleGtk::FrameStyle frame_style) {
  return frame_style == BubbleGtk::ANCHOR_TOP_LEFT ||
         frame_style == BubbleGtk::ANCHOR_TOP_MIDDLE ||
         frame_style == BubbleGtk::ANCHOR_TOP_RIGHT;
}

bool IsArrowBottom(BubbleGtk::FrameStyle frame_style) {
  return frame_style == BubbleGtk::ANCHOR_BOTTOM_LEFT ||
         frame_style == BubbleGtk::ANCHOR_BOTTOM_MIDDLE ||
         frame_style == BubbleGtk::ANCHOR_BOTTOM_RIGHT;
}

bool IsFixed(BubbleGtk::FrameStyle frame_style) {
  return frame_style == BubbleGtk::FIXED_TOP_LEFT ||
         frame_style == BubbleGtk::FIXED_TOP_RIGHT;
}

BubbleGtk::FrameStyle AdjustFrameStyleForLocale(
    BubbleGtk::FrameStyle frame_style) {
  // Only RTL requires more work.
  if (!base::i18n::IsRTL())
    return frame_style;

  switch (frame_style) {
    // These don't flip.
    case BubbleGtk::ANCHOR_TOP_MIDDLE:
    case BubbleGtk::ANCHOR_BOTTOM_MIDDLE:
    case BubbleGtk::FLOAT_BELOW_RECT:
    case BubbleGtk::CENTER_OVER_RECT:
      return frame_style;

    // These do flip.
    case BubbleGtk::ANCHOR_TOP_LEFT:
      return BubbleGtk::ANCHOR_TOP_RIGHT;

    case BubbleGtk::ANCHOR_TOP_RIGHT:
      return BubbleGtk::ANCHOR_TOP_LEFT;

    case BubbleGtk::ANCHOR_BOTTOM_LEFT:
      return BubbleGtk::ANCHOR_BOTTOM_RIGHT;

    case BubbleGtk::ANCHOR_BOTTOM_RIGHT:
      return BubbleGtk::ANCHOR_BOTTOM_LEFT;

    case BubbleGtk::FIXED_TOP_LEFT:
      return BubbleGtk::FIXED_TOP_RIGHT;

    case BubbleGtk::FIXED_TOP_RIGHT:
      return BubbleGtk::FIXED_TOP_LEFT;
  }

  NOTREACHED();
  return BubbleGtk::ANCHOR_TOP_LEFT;
}

}  // namespace

// static
BubbleGtk* BubbleGtk::Show(GtkWidget* anchor_widget,
                           const gfx::Rect* rect,
                           GtkWidget* content,
                           FrameStyle frame_style,
                           int attribute_flags,
                           GtkThemeService* provider,
                           BubbleDelegateGtk* delegate) {
  BubbleGtk* bubble = new BubbleGtk(provider,
                                    AdjustFrameStyleForLocale(frame_style),
                                    attribute_flags);
  bubble->Init(anchor_widget, rect, content, attribute_flags);
  bubble->set_delegate(delegate);
  return bubble;
}

BubbleGtk::BubbleGtk(GtkThemeService* provider,
                     FrameStyle frame_style,
                     int attribute_flags)
    : delegate_(NULL),
      window_(NULL),
      theme_service_(provider),
      accel_group_(gtk_accel_group_new()),
      toplevel_window_(NULL),
      anchor_widget_(NULL),
      mask_region_(NULL),
      requested_frame_style_(frame_style),
      actual_frame_style_(ANCHOR_TOP_LEFT),
      match_system_theme_(attribute_flags & MATCH_SYSTEM_THEME),
      grab_input_(attribute_flags & GRAB_INPUT),
      closed_by_escape_(false) {
}

BubbleGtk::~BubbleGtk() {
  // Notify the delegate that we're about to close.  This gives the chance
  // to save state / etc from the hosted widget before it's destroyed.
  if (delegate_)
    delegate_->BubbleClosing(this, closed_by_escape_);

  g_object_unref(accel_group_);
  if (mask_region_)
    gdk_region_destroy(mask_region_);
}

void BubbleGtk::Init(GtkWidget* anchor_widget,
                     const gfx::Rect* rect,
                     GtkWidget* content,
                     int attribute_flags) {
  // If there is a current grab widget (menu, other bubble, etc.), hide it.
  GtkWidget* current_grab_widget = gtk_grab_get_current();
  if (current_grab_widget)
    gtk_widget_hide(current_grab_widget);

  DCHECK(!window_);
  anchor_widget_ = anchor_widget;
  toplevel_window_ = gtk_widget_get_toplevel(anchor_widget_);
  DCHECK(gtk_widget_is_toplevel(toplevel_window_));
  rect_ = rect ? *rect : gtk_util::WidgetBounds(anchor_widget);

  // Using a TOPLEVEL window may cause placement issues with certain WMs but it
  // is necessary to be able to focus the window.
  window_ = gtk_window_new(attribute_flags & POPUP_WINDOW ?
                           GTK_WINDOW_POPUP : GTK_WINDOW_TOPLEVEL);

  gtk_widget_set_app_paintable(window_, TRUE);
  // Resizing is handled by the program, not user.
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);

  if (!(attribute_flags & NO_ACCELERATORS)) {
    // Attach all of the accelerators to the bubble.
    for (BubbleAcceleratorsGtk::const_iterator
             i(BubbleAcceleratorsGtk::begin());
         i != BubbleAcceleratorsGtk::end();
         ++i) {
      gtk_accel_group_connect(accel_group_,
                              i->keyval,
                              i->modifier_type,
                              GtkAccelFlags(0),
                              g_cclosure_new(G_CALLBACK(&OnGtkAcceleratorThunk),
                                             this,
                                             NULL));
    }
    gtk_window_add_accel_group(GTK_WINDOW(window_), accel_group_);
  }

  // |requested_frame_style_| is used instead of |actual_frame_style_| here
  // because |actual_frame_style_| is only correct after calling
  // |UpdateFrameStyle()|. Unfortunately, |UpdateFrameStyle()| requires knowing
  // the size of |window_| (which happens later on).
  int arrow_padding = HasArrow(requested_frame_style_) ? kArrowSize : 0;
  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), arrow_padding, 0, 0, 0);

  gtk_container_add(GTK_CONTAINER(alignment), content);
  gtk_container_add(GTK_CONTAINER(window_), alignment);

  // GtkWidget only exposes the bitmap mask interface.  Use GDK to more
  // efficently mask a GdkRegion.  Make sure the window is realized during
  // OnSizeAllocate, so the mask can be applied to the GdkWindow.
  gtk_widget_realize(window_);

  UpdateFrameStyle(true);  // Force move and reshape.
  StackWindow();

  gtk_widget_add_events(window_, GDK_BUTTON_PRESS_MASK);

  // Connect during the bubbling phase so the border is always on top.
  signals_.ConnectAfter(window_, "expose-event",
                        G_CALLBACK(OnExposeThunk), this);
  signals_.Connect(window_, "size-allocate", G_CALLBACK(OnSizeAllocateThunk),
                   this);
  signals_.Connect(window_, "button-press-event",
                   G_CALLBACK(OnButtonPressThunk), this);
  signals_.Connect(window_, "destroy", G_CALLBACK(OnDestroyThunk), this);
  signals_.Connect(window_, "hide", G_CALLBACK(OnHideThunk), this);
  if (grab_input_) {
    signals_.Connect(window_, "grab-broken-event",
                     G_CALLBACK(OnGrabBrokenThunk), this);
  }

  // If the toplevel window is being used as the anchor, then the signals below
  // are enough to keep us positioned correctly.
  if (anchor_widget_ != toplevel_window_) {
    signals_.Connect(anchor_widget_, "size-allocate",
                     G_CALLBACK(OnAnchorAllocateThunk), this);
    signals_.Connect(anchor_widget_, "destroy",
                     G_CALLBACK(OnAnchorDestroyThunk), this);
  }

  signals_.Connect(toplevel_window_, "configure-event",
                   G_CALLBACK(OnToplevelConfigureThunk), this);
  signals_.Connect(toplevel_window_, "unmap-event",
                   G_CALLBACK(OnToplevelUnmapThunk), this);
  signals_.Connect(window_, "destroy",
                   G_CALLBACK(OnToplevelDestroyThunk), this);

  gtk_widget_show_all(window_);

  if (grab_input_)
    gtk_grab_add(window_);
  GrabPointerAndKeyboard();

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);
}

// NOTE: This seems a bit overcomplicated, but it requires a bunch of careful
// fudging to get the pixels rasterized exactly where we want them, the arrow to
// have a 1 pixel point, etc.
// TODO(deanm): Windows draws with Skia and uses some PNG images for the
// corners.  This is a lot more work, but they get anti-aliasing.
// static
std::vector<GdkPoint> BubbleGtk::MakeFramePolygonPoints(
    FrameStyle frame_style,
    int width,
    int height,
    FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  std::vector<GdkPoint> points;

  int top_arrow_size = IsArrowTop(frame_style) ? kArrowSize : 0;
  int bottom_arrow_size = IsArrowBottom(frame_style) ? kArrowSize : 0;
  bool on_left = IsArrowLeft(frame_style);

  // If we're stroking the frame, we need to offset some of our points by 1
  // pixel.  We do this when we draw horizontal lines that are on the bottom or
  // when we draw vertical lines that are closer to the end (where "end" is the
  // right side for ANCHOR_TOP_LEFT).
  int y_off = type == FRAME_MASK ? 0 : -1;
  // We use this one for arrows located on the left.
  int x_off_l = on_left ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !on_left ? -y_off : 0;

  // Top left corner.
  points.push_back(MakeBidiGdkPoint(
      x_off_r, top_arrow_size + kCornerSize - 1, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_r - 1, top_arrow_size, width, on_left));

  // The top arrow.
  if (top_arrow_size) {
    int arrow_x = frame_style == ANCHOR_TOP_MIDDLE ? width / 2 : kArrowX;
    points.push_back(MakeBidiGdkPoint(
        arrow_x - top_arrow_size + x_off_r, top_arrow_size, width, on_left));
    points.push_back(MakeBidiGdkPoint(
        arrow_x + x_off_r, 0, width, on_left));
    points.push_back(MakeBidiGdkPoint(
        arrow_x + 1 + x_off_l, 0, width, on_left));
    points.push_back(MakeBidiGdkPoint(
        arrow_x + top_arrow_size + 1 + x_off_l, top_arrow_size,
        width, on_left));
  }

  // Top right corner.
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + 1 + x_off_l, top_arrow_size, width, on_left));
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, top_arrow_size + kCornerSize - 1, width, on_left));

  // Bottom right corner.
  points.push_back(MakeBidiGdkPoint(
      width + x_off_l, height - bottom_arrow_size - kCornerSize,
      width, on_left));
  points.push_back(MakeBidiGdkPoint(
      width - kCornerSize + x_off_r, height - bottom_arrow_size + y_off,
      width, on_left));

  // The bottom arrow.
  if (bottom_arrow_size) {
    int arrow_x = frame_style == ANCHOR_BOTTOM_MIDDLE ?
        width / 2 : kArrowX;
    points.push_back(MakeBidiGdkPoint(
        arrow_x + bottom_arrow_size + 1 + x_off_l,
        height - bottom_arrow_size + y_off,
        width,
        on_left));
    points.push_back(MakeBidiGdkPoint(
        arrow_x + 1 + x_off_l, height + y_off, width, on_left));
    points.push_back(MakeBidiGdkPoint(
        arrow_x + x_off_r, height + y_off, width, on_left));
    points.push_back(MakeBidiGdkPoint(
        arrow_x - bottom_arrow_size + x_off_r,
        height - bottom_arrow_size + y_off,
        width,
        on_left));
  }

  // Bottom left corner.
  points.push_back(MakeBidiGdkPoint(
      kCornerSize + x_off_l, height -bottom_arrow_size + y_off,
      width, on_left));
  points.push_back(MakeBidiGdkPoint(
      x_off_r, height - bottom_arrow_size - kCornerSize, width, on_left));

  return points;
}

BubbleGtk::FrameStyle BubbleGtk::GetAllowedFrameStyle(
    FrameStyle preferred_style,
    int arrow_x,
    int arrow_y,
    int width,
    int height) {
  if (IsFixed(preferred_style))
    return preferred_style;

  const int screen_width = gdk_screen_get_width(gdk_screen_get_default());
  const int screen_height = gdk_screen_get_height(gdk_screen_get_default());

  // Choose whether to show the bubble above or below the specified location.
  const bool prefer_top_arrow = IsArrowTop(preferred_style) ||
             preferred_style == FLOAT_BELOW_RECT;
  // The bleed measures the amount of bubble that would be shown offscreen.
  const int top_arrow_bleed =
      std::max(height + kArrowSize + arrow_y - screen_height, 0);
  const int bottom_arrow_bleed = std::max(height + kArrowSize - arrow_y, 0);

  FrameStyle frame_style_none = FLOAT_BELOW_RECT;
  FrameStyle frame_style_left = ANCHOR_TOP_LEFT;
  FrameStyle frame_style_middle = ANCHOR_TOP_MIDDLE;
  FrameStyle frame_style_right = ANCHOR_TOP_RIGHT;
  if ((prefer_top_arrow && (top_arrow_bleed > bottom_arrow_bleed)) ||
      (!prefer_top_arrow && (top_arrow_bleed >= bottom_arrow_bleed))) {
    frame_style_none = CENTER_OVER_RECT;
    frame_style_left = ANCHOR_BOTTOM_LEFT;
    frame_style_middle = ANCHOR_BOTTOM_MIDDLE;
    frame_style_right = ANCHOR_BOTTOM_RIGHT;
  }

  if (!HasArrow(preferred_style))
    return frame_style_none;

  if (IsArrowMiddle(preferred_style))
    return frame_style_middle;

  // Choose whether to show the bubble left or right of the specified location.
  const bool prefer_left_arrow = IsArrowLeft(preferred_style);
  // The bleed measures the amount of bubble that would be shown offscreen.
  const int left_arrow_bleed =
      std::max(width + arrow_x - kArrowX - screen_width, 0);
  const int right_arrow_bleed = std::max(width - arrow_x - kArrowX, 0);

  // Use the preferred location if it doesn't bleed more than the opposite side.
  return ((prefer_left_arrow && (left_arrow_bleed <= right_arrow_bleed)) ||
          (!prefer_left_arrow && (left_arrow_bleed < right_arrow_bleed))) ?
      frame_style_left : frame_style_right;
}

bool BubbleGtk::UpdateFrameStyle(bool force_move_and_reshape) {
  if (!toplevel_window_ || !anchor_widget_)
    return false;

  gint toplevel_x = 0, toplevel_y = 0;
  gdk_window_get_position(gtk_widget_get_window(toplevel_window_),
                          &toplevel_x, &toplevel_y);
  int offset_x, offset_y;
  gtk_widget_translate_coordinates(anchor_widget_, toplevel_window_,
                                   rect_.x(), rect_.y(), &offset_x, &offset_y);

  FrameStyle old_frame_style = actual_frame_style_;
  GtkAllocation allocation;
  gtk_widget_get_allocation(window_, &allocation);
  actual_frame_style_ = GetAllowedFrameStyle(
      requested_frame_style_,
      toplevel_x + offset_x + (rect_.width() / 2),  // arrow_x
      toplevel_y + offset_y,
      allocation.width,
      allocation.height);

  if (force_move_and_reshape || actual_frame_style_ != old_frame_style) {
    UpdateWindowShape();
    MoveWindow();
    // We need to redraw the entire window to repaint its border.
    gtk_widget_queue_draw(window_);
    return true;
  }
  return false;
}

void BubbleGtk::UpdateWindowShape() {
  if (mask_region_) {
    gdk_region_destroy(mask_region_);
    mask_region_ = NULL;
  }
  GtkAllocation allocation;
  gtk_widget_get_allocation(window_, &allocation);
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      actual_frame_style_, allocation.width, allocation.height,
      FRAME_MASK);
  mask_region_ = gdk_region_polygon(&points[0],
                                    points.size(),
                                    GDK_EVEN_ODD_RULE);

  GdkWindow* gdk_window = gtk_widget_get_window(window_);
  gdk_window_shape_combine_region(gdk_window, NULL, 0, 0);
  gdk_window_shape_combine_region(gdk_window, mask_region_, 0, 0);
}

void BubbleGtk::MoveWindow() {
  if (!toplevel_window_ || !anchor_widget_)
    return;

  gint toplevel_x = 0, toplevel_y = 0;
  gdk_window_get_position(gtk_widget_get_window(toplevel_window_),
                          &toplevel_x, &toplevel_y);

  int offset_x, offset_y;
  gtk_widget_translate_coordinates(anchor_widget_, toplevel_window_,
                                   rect_.x(), rect_.y(), &offset_x, &offset_y);

  gint screen_x = 0;
  if (IsFixed(actual_frame_style_)) {
    GtkAllocation toplevel_allocation;
    gtk_widget_get_allocation(toplevel_window_, &toplevel_allocation);

    GtkAllocation bubble_allocation;
    gtk_widget_get_allocation(window_, &bubble_allocation);

    int x_offset = actual_frame_style_ == FIXED_TOP_LEFT ?
        kFixedPositionPaddingEnd :
        toplevel_allocation.width - bubble_allocation.width -
            kFixedPositionPaddingEnd;
    screen_x = toplevel_x + x_offset;
  } else if (!HasArrow(actual_frame_style_) ||
             IsArrowMiddle(actual_frame_style_)) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(window_, &allocation);
    screen_x =
        toplevel_x + offset_x + (rect_.width() / 2) - allocation.width / 2;
  } else if (IsArrowLeft(actual_frame_style_)) {
    screen_x = toplevel_x + offset_x + (rect_.width() / 2) - kArrowX;
  } else if (IsArrowRight(actual_frame_style_)) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(window_, &allocation);
    screen_x = toplevel_x + offset_x + (rect_.width() / 2) -
               allocation.width + kArrowX;
  } else {
    NOTREACHED();
  }

  gint screen_y = toplevel_y + offset_y + rect_.height();
  if (IsFixed(actual_frame_style_)) {
    screen_y = toplevel_y + kFixedPositionPaddingTop;
  } else if (IsArrowTop(actual_frame_style_) ||
             actual_frame_style_ == FLOAT_BELOW_RECT) {
    screen_y += kArrowToContentPadding;
  } else {
    GtkAllocation allocation;
    gtk_widget_get_allocation(window_, &allocation);
    screen_y -= allocation.height + kArrowToContentPadding;
  }

  gtk_window_move(GTK_WINDOW(window_), screen_x, screen_y);
}

void BubbleGtk::StackWindow() {
  // Stack our window directly above the toplevel window.
  if (toplevel_window_)
    ui::StackPopupWindow(window_, toplevel_window_);
}

void BubbleGtk::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_BROWSER_THEME_CHANGED);
  if (theme_service_->UsingNativeTheme() && match_system_theme_) {
    gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, NULL);
  } else {
    // Set the background color, so we don't need to paint it manually.
    gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kBackgroundColor);
  }
}

void BubbleGtk::StopGrabbingInput() {
  if (!grab_input_)
    return;
  grab_input_ = false;
  gtk_grab_remove(window_);
}

GtkWindow* BubbleGtk::GetNativeWindow() {
  return GTK_WINDOW(window_);
}

void BubbleGtk::Close() {
  // We don't need to ungrab the pointer or keyboard here; the X server will
  // automatically do that when we destroy our window.
  DCHECK(window_);
  gtk_widget_destroy(window_);
  // |this| has been deleted, see OnDestroy.
}

void BubbleGtk::GrabPointerAndKeyboard() {
  GdkWindow* gdk_window = gtk_widget_get_window(window_);

  // Install X pointer and keyboard grabs to make sure that we have the focus
  // and get all mouse and keyboard events until we're closed. As a hack, grab
  // the pointer even if |grab_input_| is false to prevent a weird error
  // rendering the bubble's frame. See
  // https://code.google.com/p/chromium/issues/detail?id=130820.
  GdkGrabStatus pointer_grab_status =
      gdk_pointer_grab(gdk_window,
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

  // Only grab the keyboard input if |grab_input_| is true.
  if (grab_input_) {
    GdkGrabStatus keyboard_grab_status =
        gdk_keyboard_grab(gdk_window,
                          FALSE,  // owner_events
                          GDK_CURRENT_TIME);
    if (keyboard_grab_status != GDK_GRAB_SUCCESS) {
      DLOG(ERROR) << "Unable to grab keyboard (status="
                  << keyboard_grab_status << ")";
    }
  }
}

gboolean BubbleGtk::OnGtkAccelerator(GtkAccelGroup* group,
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
        gdk_keymap_get_entries_for_keyval(NULL,
                                          keyval,
                                          &keys,
                                          &n_keys);
        if (n_keys) {
          // Forward the accelerator to root window the bubble is anchored
          // to for further processing
          msg.type = GDK_KEY_PRESS;
          msg.window = gtk_widget_get_window(toplevel_window_);
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
        Close();
      }
      break;
    default:
      return FALSE;
  }

  return TRUE;
}

gboolean BubbleGtk::OnExpose(GtkWidget* widget, GdkEventExpose* expose) {
  // TODO(erg): This whole method will need to be rewritten in cairo.
  GdkDrawable* drawable = GDK_DRAWABLE(gtk_widget_get_window(window_));
  GdkGC* gc = gdk_gc_new(drawable);
  gdk_gc_set_rgb_fg_color(gc, &kFrameColor);

  // Stroke the frame border.
  GtkAllocation allocation;
  gtk_widget_get_allocation(window_, &allocation);
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      actual_frame_style_, allocation.width, allocation.height,
      FRAME_STROKE);
  gdk_draw_polygon(drawable, gc, FALSE, &points[0], points.size());

  // If |grab_input_| is false, pointer input has been grabbed as a hack in
  // |GrabPointerAndKeyboard()| to ensure that the polygon frame is drawn
  // correctly. Since the intention is not actually to grab the pointer, release
  // it now that the frame is drawn to prevent clicks from being missed. See
  // https://code.google.com/p/chromium/issues/detail?id=130820.
  if (!grab_input_)
    gdk_pointer_ungrab(GDK_CURRENT_TIME);

  g_object_unref(gc);
  return FALSE;  // Propagate so our children paint, etc.
}

// When our size is initially allocated or changed, we need to recompute
// and apply our shape mask region.
void BubbleGtk::OnSizeAllocate(GtkWidget* widget,
                               GtkAllocation* allocation) {
  if (!UpdateFrameStyle(false)) {
    UpdateWindowShape();
    MoveWindow();
  }
}

gboolean BubbleGtk::OnButtonPress(GtkWidget* widget,
                                  GdkEventButton* event) {
  // If we got a click in our own window, that's okay (we need to additionally
  // check that it falls within our bounds, since we've grabbed the pointer and
  // some events that actually occurred in other windows will be reported with
  // respect to our window).
  GdkWindow* gdk_window = gtk_widget_get_window(window_);
  if (event->window == gdk_window &&
      (mask_region_ && gdk_region_point_in(mask_region_, event->x, event->y))) {
    return FALSE;  // Propagate.
  }

  // Our content widget got a click.
  if (event->window != gdk_window &&
      gdk_window_get_toplevel(event->window) == gdk_window) {
    return FALSE;
  }

  if (grab_input_) {
    // Otherwise we had a click outside of our window, close ourself.
    Close();
    return TRUE;
  }

  return FALSE;
}

gboolean BubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroy the widget manually, or the window was closed via X.  This will
  // delete the BubbleGtk object.
  delete this;
  return FALSE;  // Propagate.
}

void BubbleGtk::OnHide(GtkWidget* widget) {
  gtk_widget_destroy(widget);
}

gboolean BubbleGtk::OnGrabBroken(GtkWidget* widget,
                                 GdkEventGrabBroken* grab_broken) {
  // |grab_input_| may have been changed to false.
  if (!grab_input_)
    return false;

  gpointer user_data;
  gdk_window_get_user_data(grab_broken->grab_window, &user_data);

  if (GTK_IS_WIDGET(user_data)) {
    signals_.Connect(GTK_WIDGET(user_data), "hide",
                     G_CALLBACK(OnForeshadowWidgetHideThunk), this);
  }

  return FALSE;
}

void BubbleGtk::OnForeshadowWidgetHide(GtkWidget* widget) {
  if (grab_input_)
    GrabPointerAndKeyboard();

  signals_.DisconnectAll(widget);
}

gboolean BubbleGtk::OnToplevelConfigure(GtkWidget* widget,
                                        GdkEventConfigure* event) {
  if (!UpdateFrameStyle(false))
    MoveWindow();
  StackWindow();
  return FALSE;
}

gboolean BubbleGtk::OnToplevelUnmap(GtkWidget* widget, GdkEvent* event) {
  Close();
  return FALSE;
}

void BubbleGtk::OnAnchorAllocate(GtkWidget* widget,
                                 GtkAllocation* allocation) {
  if (!UpdateFrameStyle(false))
    MoveWindow();
}

void BubbleGtk::OnAnchorDestroy(GtkWidget* widget) {
  anchor_widget_ = NULL;
  Close();
  // |this| is deleted.
}

void BubbleGtk::OnToplevelDestroy(GtkWidget* widget) {
  toplevel_window_ = NULL;
  Close();
  // |this| is deleted.
}
