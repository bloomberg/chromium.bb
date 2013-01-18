// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/panels/panel_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/gtk_window_util.h"
#include "chrome/browser/ui/gtk/panels/panel_titlebar_gtk.h"
#include "chrome/browser/ui/gtk/panels/panel_drag_gtk.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/accelerators/platform_accelerator_gtk.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_expanded_container.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/x/active_window_watcher_x.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/cairo_cached_surface.h"
#include "ui/gfx/image/image.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;

namespace {

const char* kPanelWindowKey = "__PANEL_GTK__";

// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;

// The frame border is only visible in restored mode and is hardcoded to 4 px
// on each side regardless of the system window border size.
const int kFrameBorderThickness = 4;
// While resize areas on Windows are normally the same size as the window
// borders, our top area is shrunk by 1 px to make it easier to move the window
// around with our thinner top grabbable strip.  (Incidentally, our side and
// bottom resize areas don't match the frame border thickness either -- they
// span the whole nonclient area, so there's no "dead zone" for the mouse.)
const int kTopResizeAdjust = 1;
// In the window corners, the resize areas don't actually expand bigger, but
// the 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;

// Colors used to draw frame background under default theme.
const SkColor kActiveBackgroundDefaultColor = SkColorSetRGB(0x3a, 0x3d, 0x3d);
const SkColor kInactiveBackgroundDefaultColor = SkColorSetRGB(0x7a, 0x7c, 0x7c);
const SkColor kAttentionBackgroundDefaultColor =
    SkColorSetRGB(0xff, 0xab, 0x57);
const SkColor kMinimizeBackgroundDefaultColor = SkColorSetRGB(0xf5, 0xf4, 0xf0);
const SkColor kMinimizeBorderDefaultColor = SkColorSetRGB(0xc9, 0xc9, 0xc9);

// Set minimium width for window really small.
const int kMinWindowWidth = 26;

// Table of supported accelerators in Panels.
const struct AcceleratorMapping {
  guint keyval;
  int command_id;
  GdkModifierType modifier_type;
} kAcceleratorMap[] = {
  // Window controls.
  { GDK_w, IDC_CLOSE_WINDOW, GDK_CONTROL_MASK },
  { GDK_w, IDC_CLOSE_WINDOW,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_q, IDC_EXIT, GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },

  // Zoom level.
  { GDK_KP_Add, IDC_ZOOM_PLUS, GDK_CONTROL_MASK },
  { GDK_plus, IDC_ZOOM_PLUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_equal, IDC_ZOOM_PLUS, GDK_CONTROL_MASK },
  { XF86XK_ZoomIn, IDC_ZOOM_PLUS, GdkModifierType(0) },
  { GDK_KP_0, IDC_ZOOM_NORMAL, GDK_CONTROL_MASK },
  { GDK_0, IDC_ZOOM_NORMAL, GDK_CONTROL_MASK },
  { GDK_KP_Subtract, IDC_ZOOM_MINUS, GDK_CONTROL_MASK },
  { GDK_minus, IDC_ZOOM_MINUS, GDK_CONTROL_MASK },
  { GDK_underscore, IDC_ZOOM_MINUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { XF86XK_ZoomOut, IDC_ZOOM_MINUS, GdkModifierType(0) },

  // Navigation.
  { GDK_Escape, IDC_STOP, GdkModifierType(0) },
  { XF86XK_Stop, IDC_STOP, GdkModifierType(0) },
  { GDK_r, IDC_RELOAD, GDK_CONTROL_MASK },
  { GDK_r, IDC_RELOAD_IGNORING_CACHE,
    GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK) },
  { GDK_F5, IDC_RELOAD, GdkModifierType(0) },
  { GDK_F5, IDC_RELOAD_IGNORING_CACHE, GDK_CONTROL_MASK },
  { GDK_F5, IDC_RELOAD_IGNORING_CACHE, GDK_SHIFT_MASK },
  { XF86XK_Reload, IDC_RELOAD, GdkModifierType(0) },
  { XF86XK_Refresh, IDC_RELOAD, GdkModifierType(0) },

  // Editing.
  { GDK_c, IDC_COPY, GDK_CONTROL_MASK },
  { GDK_x, IDC_CUT, GDK_CONTROL_MASK },
  { GDK_v, IDC_PASTE, GDK_CONTROL_MASK },

  // Dev tools.
  { GDK_i, IDC_DEV_TOOLS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_j, IDC_DEV_TOOLS_CONSOLE,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },

};

// Table of accelerator mappings to command ids.
typedef std::map<ui::Accelerator, int> AcceleratorMap;

const AcceleratorMap& GetAcceleratorTable() {
  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, accelerator_table, ());
  if (accelerator_table.empty()) {
    for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
      const AcceleratorMapping& entry = kAcceleratorMap[i];
      ui::Accelerator accelerator = ui::AcceleratorForGdkKeyCodeAndModifier(
          entry.keyval, entry.modifier_type);
      accelerator_table[accelerator] = entry.command_id;
    }
  }
  return accelerator_table;
}

gfx::Image CreateImageForColor(SkColor color) {
  gfx::Canvas canvas(gfx::Size(1, 1), ui::SCALE_FACTOR_100P, true);
  canvas.DrawColor(color);
  return gfx::Image(gfx::ImageSkia(canvas.ExtractImageRep()));
}

const gfx::Image GetActiveBackgroundDefaultImage() {
  CR_DEFINE_STATIC_LOCAL(gfx::Image, image, ());
  if (image.IsEmpty())
    image = CreateImageForColor(kActiveBackgroundDefaultColor);
  return image;
}

gfx::Image GetInactiveBackgroundDefaultImage() {
  CR_DEFINE_STATIC_LOCAL(gfx::Image, image, ());
  if (image.IsEmpty())
    image = CreateImageForColor(kInactiveBackgroundDefaultColor);
  return image;
}

gfx::Image GetAttentionBackgroundDefaultImage() {
  CR_DEFINE_STATIC_LOCAL(gfx::Image, image, ());
  if (image.IsEmpty())
    image = CreateImageForColor(kAttentionBackgroundDefaultColor);
  return image;
}

gfx::Image GetMinimizeBackgroundDefaultImage() {
  CR_DEFINE_STATIC_LOCAL(gfx::Image, image, ());
  if (image.IsEmpty())
    image = CreateImageForColor(kMinimizeBackgroundDefaultColor);
  return image;
}

// Used to stash a pointer to the Panel window inside the native
// Gtk window for retrieval in static callbacks.
GQuark GetPanelWindowQuarkKey() {
  static GQuark quark = g_quark_from_static_string(kPanelWindowKey);
  return quark;
}

// Size of window frame. Empty until first panel has been allocated
// and sized. Frame size won't change for other panels so it can be
// computed once for all panels.
gfx::Size& GetFrameSize() {
  CR_DEFINE_STATIC_LOCAL(gfx::Size, frame_size, ());
  return frame_size;
}

void SetFrameSize(const gfx::Size& new_size) {
  gfx::Size& frame_size = GetFrameSize();
  frame_size.SetSize(new_size.width(), new_size.height());
}

}

// static
NativePanel* Panel::CreateNativePanel(Panel* panel, const gfx::Rect& bounds) {
  PanelGtk* panel_gtk = new PanelGtk(panel, bounds);
  panel_gtk->Init();
  return panel_gtk;
}

PanelGtk::PanelGtk(Panel* panel, const gfx::Rect& bounds)
    : panel_(panel),
      bounds_(bounds),
      always_on_top_(false),
      is_shown_(false),
      paint_state_(PAINT_AS_INACTIVE),
      is_drawing_attention_(false),
      frame_cursor_(NULL),
      is_active_(!ui::ActiveWindowWatcherX::WMSupportsActivation()),
      window_(NULL),
      window_container_(NULL),
      window_vbox_(NULL),
      render_area_event_box_(NULL),
      contents_expanded_(NULL),
      accel_group_(NULL) {
}

PanelGtk::~PanelGtk() {
  ui::ActiveWindowWatcherX::RemoveObserver(this);
}

void PanelGtk::Init() {
  ui::ActiveWindowWatcherX::AddObserver(this);

  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  g_object_set_qdata(G_OBJECT(window_), GetPanelWindowQuarkKey(), this);
  gtk_widget_add_events(GTK_WIDGET(window_), GDK_BUTTON_PRESS_MASK |
                                             GDK_POINTER_MOTION_MASK);
  gtk_window_set_decorated(window_, false);

  // Disable the resize gripper on Ubuntu.
  gtk_window_util::DisableResizeGrip(window_);

  // Add this window to its own unique window group to allow for
  // window-to-parent modality.
  gtk_window_group_add_window(gtk_window_group_new(), window_);
  g_object_unref(gtk_window_get_group(window_));

  // Set minimum height for the window.
  GdkGeometry hints;
  hints.min_height = panel::kMinimizedPanelHeight;
  hints.min_width = kMinWindowWidth;
  gtk_window_set_geometry_hints(
      window_, GTK_WIDGET(window_), &hints, GDK_HINT_MIN_SIZE);

  // Connect signal handlers to the window.
  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnMainWindowDeleteEventThunk), this);
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(OnMainWindowDestroyThunk), this);
  g_signal_connect(window_, "configure-event",
                   G_CALLBACK(OnConfigureThunk), this);
  g_signal_connect(window_, "key-press-event",
                   G_CALLBACK(OnKeyPressThunk), this);
  g_signal_connect(window_, "motion-notify-event",
                   G_CALLBACK(OnMouseMoveEventThunk), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(OnButtonPressEventThunk), this);

  // This vbox contains the titlebar and the render area, but not
  // the custom frame border.
  window_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(window_vbox_);

  // TODO(jennb): add GlobalMenuBar after refactoring out Browser.

  // The window container draws the custom window frame.
  window_container_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_widget_set_name(window_container_, "chrome-custom-frame-border");
  gtk_widget_set_app_paintable(window_container_, TRUE);
  gtk_widget_set_double_buffered(window_container_, FALSE);
  gtk_widget_set_redraw_on_allocate(window_container_, TRUE);
  gtk_alignment_set_padding(GTK_ALIGNMENT(window_container_), 0,
      kFrameBorderThickness, kFrameBorderThickness, kFrameBorderThickness);
  g_signal_connect(window_container_, "expose-event",
                   G_CALLBACK(OnCustomFrameExposeThunk), this);
  gtk_container_add(GTK_CONTAINER(window_container_), window_vbox_);

  // Build the titlebar.
  titlebar_.reset(new PanelTitlebarGtk(this));
  titlebar_->Init();
  gtk_box_pack_start(GTK_BOX(window_vbox_), titlebar_->widget(), FALSE, FALSE,
                     0);
  g_signal_connect(titlebar_->widget(), "button-press-event",
                   G_CALLBACK(OnTitlebarButtonPressEventThunk), this);
  g_signal_connect(titlebar_->widget(), "button-release-event",
                   G_CALLBACK(OnTitlebarButtonReleaseEventThunk), this);

  contents_expanded_ = gtk_expanded_container_new();
  gtk_widget_show(contents_expanded_);

  render_area_event_box_ = gtk_event_box_new();
  // Set a white background so during startup the user sees white in the
  // content area before we get a WebContents in place.
  gtk_widget_modify_bg(render_area_event_box_, GTK_STATE_NORMAL,
                       &ui::kGdkWhite);
  gtk_container_add(GTK_CONTAINER(render_area_event_box_),
                    contents_expanded_);
  gtk_widget_show(render_area_event_box_);
  gtk_box_pack_end(GTK_BOX(window_vbox_), render_area_event_box_,
                   TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(window_), window_container_);
  gtk_widget_show(window_container_);

  ConnectAccelerators();
}

void PanelGtk::UpdateWindowShape(int width, int height) {
  // For panels, only top corners are rounded. The bottom corners are not
  // rounded because panels are aligned to the bottom edge of the screen.
  GdkRectangle top_top_rect = { 3, 0, width - 6, 1 };
  GdkRectangle top_mid_rect = { 1, 1, width - 2, 2 };
  GdkRectangle mid_rect = { 0, 3, width, height - 3 };
  GdkRegion* mask = gdk_region_rectangle(&top_top_rect);
  gdk_region_union_with_rect(mask, &top_mid_rect);
  gdk_region_union_with_rect(mask, &mid_rect);
  gdk_window_shape_combine_region(
      gtk_widget_get_window(GTK_WIDGET(window_)), mask, 0, 0);
  if (mask)
    gdk_region_destroy(mask);
}

gboolean PanelGtk::OnConfigure(GtkWidget* widget,
                               GdkEventConfigure* event) {
  // When the window moves, we'll get multiple configure-event signals. We can
  // also get events when the bounds haven't changed, but the window's stacking
  // has, which we aren't interested in. http://crbug.com/70125
  gfx::Size new_size(event->width, event->height);
  if (new_size == configure_size_)
    return FALSE;

  UpdateWindowShape(event->width, event->height);
  configure_size_ = new_size;

  if (!GetFrameSize().IsEmpty())
    return FALSE;

  // Save the frame size allocated by the system as the
  // frame size will be affected when we shrink the panel smaller
  // than the frame (e.g. when the panel is minimized).
  SetFrameSize(GetNonClientFrameSize());
  panel_->OnWindowSizeAvailable();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_WINDOW_SIZE_KNOWN,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());

  return FALSE;
}

void PanelGtk::ConnectAccelerators() {
  accel_group_ = gtk_accel_group_new();
  gtk_window_add_accel_group(window_, accel_group_);

  const AcceleratorMap& accelerator_table = GetAcceleratorTable();
  for (AcceleratorMap::const_iterator iter = accelerator_table.begin();
       iter != accelerator_table.end(); ++iter) {
    gtk_accel_group_connect(
        accel_group_,
        ui::GetGdkKeyCodeForAccelerator(iter->first),
        ui::GetGdkModifierForAccelerator(iter->first),
        GtkAccelFlags(0),
        g_cclosure_new(G_CALLBACK(OnGtkAccelerator),
                       GINT_TO_POINTER(iter->second), NULL));
  }
}

void PanelGtk::DisconnectAccelerators() {
  // Disconnecting the keys we connected to our accelerator group frees the
  // closures allocated in ConnectAccelerators.
  const AcceleratorMap& accelerator_table = GetAcceleratorTable();
  for (AcceleratorMap::const_iterator iter = accelerator_table.begin();
       iter != accelerator_table.end(); ++iter) {
    gtk_accel_group_disconnect_key(
        accel_group_,
        ui::GetGdkKeyCodeForAccelerator(iter->first),
        ui::GetGdkModifierForAccelerator(iter->first));
  }
  gtk_window_remove_accel_group(window_, accel_group_);
  g_object_unref(accel_group_);
  accel_group_ = NULL;
}

// static
gboolean PanelGtk::OnGtkAccelerator(GtkAccelGroup* accel_group,
                                    GObject* acceleratable,
                                    guint keyval,
                                    GdkModifierType modifier,
                                    void* user_data) {
  DCHECK(acceleratable);
  int command_id = GPOINTER_TO_INT(user_data);
  PanelGtk* panel_gtk = static_cast<PanelGtk*>(
      g_object_get_qdata(acceleratable, GetPanelWindowQuarkKey()));
  return panel_gtk->panel()->ExecuteCommandIfEnabled(command_id);
}

gboolean PanelGtk::OnKeyPress(GtkWidget* widget, GdkEventKey* event) {
  // No way to deactivate a window in GTK, so ignore input if window
  // is supposed to be 'inactive'. See comments in DeactivatePanel().
  if (!is_active_)
    return TRUE;

  // Propagate the key event to child widget first, so we don't override
  // their accelerators.
  if (!gtk_window_propagate_key_event(GTK_WINDOW(widget), event)) {
    if (!gtk_window_activate_key(GTK_WINDOW(widget), event)) {
      gtk_bindings_activate_event(GTK_OBJECT(widget), event);
    }
  }
  return TRUE;
}

bool PanelGtk::UsingDefaultTheme() const {
  // No theme is provided for attention painting.
  if (paint_state_ == PAINT_FOR_ATTENTION)
    return true;

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(panel_->profile());
  return theme_provider->UsingDefaultTheme() ||
      theme_provider->UsingNativeTheme();
}

bool PanelGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) const {
  // Only detect the window edge when panels can be resized by the user.
  // This method is used by the base class to detect when the cursor has
  // hit the window edge in order to change the cursor to a resize cursor
  // and to detect when to initiate a resize drag.
  panel::Resizability resizability = panel_->CanResizeByMouse();
  if (panel::NOT_RESIZABLE == resizability)
    return false;

  if (x < kFrameBorderThickness) {
    // Left edge.
    if (y < kResizeAreaCornerSize - kTopResizeAdjust) {
      *edge = GDK_WINDOW_EDGE_NORTH_WEST;
    } else if (y < bounds_.height() - kResizeAreaCornerSize) {
      *edge = GDK_WINDOW_EDGE_WEST;
    } else {
      *edge = GDK_WINDOW_EDGE_SOUTH_WEST;
    }
  } else if (x < bounds_.width() - kFrameBorderThickness) {
    if (y < kFrameBorderThickness - kTopResizeAdjust) {
      // Top edge.
      if (x < kResizeAreaCornerSize) {
        *edge = GDK_WINDOW_EDGE_NORTH_WEST;
      } else if (x < bounds_.width() - kResizeAreaCornerSize) {
        *edge = GDK_WINDOW_EDGE_NORTH;
      } else {
        *edge = GDK_WINDOW_EDGE_NORTH_EAST;
      }
    } else if (y < bounds_.height() - kFrameBorderThickness) {
      // Ignore the middle content area.
      return false;
    } else {
      // Bottom edge.
      if (x < kResizeAreaCornerSize) {
        *edge = GDK_WINDOW_EDGE_SOUTH_WEST;
      } else if (x < bounds_.width() - kResizeAreaCornerSize) {
        *edge = GDK_WINDOW_EDGE_SOUTH;
      } else {
        *edge = GDK_WINDOW_EDGE_SOUTH_EAST;
      }
    }
  } else {
    // Right edge.
    if (y < kResizeAreaCornerSize - kTopResizeAdjust) {
      *edge = GDK_WINDOW_EDGE_NORTH_EAST;
    } else if (y < bounds_.height() - kResizeAreaCornerSize) {
      *edge = GDK_WINDOW_EDGE_EAST;
    } else {
      *edge = GDK_WINDOW_EDGE_SOUTH_EAST;
    }
  }

  // Special handling if bottom edge is not resizable.
  if (panel::RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM == resizability) {
    if (*edge == GDK_WINDOW_EDGE_SOUTH)
      return FALSE;
    if (*edge == GDK_WINDOW_EDGE_SOUTH_WEST)
      *edge = GDK_WINDOW_EDGE_WEST;
    else if (*edge == GDK_WINDOW_EDGE_SOUTH_EAST)
      *edge = GDK_WINDOW_EDGE_EAST;
  }

  return true;
}

gfx::Image PanelGtk::GetFrameBackground() const {
  return UsingDefaultTheme() ?
      GetDefaultFrameBackground() : GetThemedFrameBackground();
}

gfx::Image PanelGtk::GetDefaultFrameBackground() const {
  switch (paint_state_) {
    case PAINT_AS_INACTIVE:
      return GetInactiveBackgroundDefaultImage();
    case PAINT_AS_ACTIVE:
      return GetActiveBackgroundDefaultImage();
    case PAINT_AS_MINIMIZED:
      return GetMinimizeBackgroundDefaultImage();
    case PAINT_FOR_ATTENTION:
      return GetAttentionBackgroundDefaultImage();
    default:
      NOTREACHED();
      return GetInactiveBackgroundDefaultImage();
  }
}

gfx::Image PanelGtk::GetThemedFrameBackground() const {
  GtkThemeService* theme_provider = GtkThemeService::GetFrom(panel_->profile());
  return theme_provider->GetImageNamed(paint_state_ == PAINT_AS_ACTIVE ?
      IDR_THEME_TOOLBAR : IDR_THEME_TAB_BACKGROUND);
}

gboolean PanelGtk::OnCustomFrameExpose(GtkWidget* widget,
                                       GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "PanelGtk::OnCustomFrameExpose");
  cairo_t* cr = gdk_cairo_create(gtk_widget_get_window(widget));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  // Update the painting state.
  int window_height = gdk_window_get_height(gtk_widget_get_window(widget));
  if (is_drawing_attention_)
    paint_state_ = PAINT_FOR_ATTENTION;
  else if (window_height <= panel::kMinimizedPanelHeight)
    paint_state_ = PAINT_AS_MINIMIZED;
  else if (is_active_)
    paint_state_ = PAINT_AS_ACTIVE;
  else
    paint_state_ = PAINT_AS_INACTIVE;

  // Draw the background.
  gfx::CairoCachedSurface* surface = GetFrameBackground().ToCairo();
  surface->SetSource(cr, widget, 0, 0);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_fill(cr);

  // Draw the border for the minimized panel only.
  if (paint_state_ == PAINT_AS_MINIMIZED) {
    cairo_move_to(cr, 0, 3);
    cairo_line_to(cr, 1, 2);
    cairo_line_to(cr, 1, 1);
    cairo_line_to(cr, 2, 1);
    cairo_line_to(cr, 3, 0);
    cairo_line_to(cr, event->area.width - 3, 0);
    cairo_line_to(cr, event->area.width - 2, 1);
    cairo_line_to(cr, event->area.width - 1, 1);
    cairo_line_to(cr, event->area.width - 1, 2);
    cairo_line_to(cr, event->area.width - 1, 3);
    cairo_line_to(cr, event->area.width - 1, event->area.height - 1);
    cairo_line_to(cr, 0, event->area.height - 1);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr,
                         SkColorGetR(kMinimizeBorderDefaultColor) / 255.0,
                         SkColorGetG(kMinimizeBorderDefaultColor) / 255.0,
                         SkColorGetB(kMinimizeBorderDefaultColor) / 255.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
  }

  cairo_destroy(cr);

  return FALSE;  // Allow subwidgets to paint.
}

void PanelGtk::EnsureDragHelperCreated() {
  if (drag_helper_.get())
    return;

  drag_helper_.reset(new PanelDragGtk(panel_.get()));
  gtk_box_pack_end(GTK_BOX(window_vbox_), drag_helper_->widget(),
                   FALSE, FALSE, 0);
}

gboolean PanelGtk::OnTitlebarButtonPressEvent(
    GtkWidget* widget, GdkEventButton* event) {
  if (event->button != 1)
    return TRUE;
  if (event->type != GDK_BUTTON_PRESS)
    return TRUE;

  gdk_window_raise(gtk_widget_get_window(GTK_WIDGET(window_)));
  EnsureDragHelperCreated();
  drag_helper_->InitialTitlebarMousePress(event, titlebar_->widget());
  return TRUE;
}

gboolean PanelGtk::OnTitlebarButtonReleaseEvent(
    GtkWidget* widget, GdkEventButton* event) {
  if (event->button != 1)
    return TRUE;

  panel_->OnTitlebarClicked((event->state & GDK_CONTROL_MASK) ?
                            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  return TRUE;
}

gboolean PanelGtk::OnMouseMoveEvent(GtkWidget* widget,
                                    GdkEventMotion* event) {
  // This method is used to update the mouse cursor when over the edge of the
  // custom frame.  If we're over some other widget, do nothing.
  if (event->window != gtk_widget_get_window(widget)) {
    // Reset the cursor.
    if (frame_cursor_) {
      frame_cursor_ = NULL;
      gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window_)), NULL);
    }
    return FALSE;
  }

  // Update the cursor if we're on the custom frame border.
  GdkWindowEdge edge;
  bool has_hit_edge = GetWindowEdge(static_cast<int>(event->x),
                                    static_cast<int>(event->y), &edge);
  GdkCursorType new_cursor = has_hit_edge ?
      gtk_window_util::GdkWindowEdgeToGdkCursorType(edge) : GDK_LAST_CURSOR;
  GdkCursorType last_cursor =
      frame_cursor_ ? frame_cursor_->type : GDK_LAST_CURSOR;

  if (last_cursor != new_cursor) {
    frame_cursor_ = has_hit_edge ? gfx::GetCursor(new_cursor) : NULL;
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window_)),
                          frame_cursor_);
  }
  return FALSE;
}

gboolean PanelGtk::OnButtonPressEvent(GtkWidget* widget,
                                      GdkEventButton* event) {
  if (event->button != 1 || event->type != GDK_BUTTON_PRESS)
    return FALSE;

  // No way to deactivate a window in GTK, so we pretended it is deactivated.
  // See comments in DeactivatePanel().
  // Mouse click anywhere in window should re-activate window so do it now.
  if (!is_active_)
    panel_->Activate();

  // Make the button press coordinate relative to the panel window.
  int win_x, win_y;
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window_));
  gdk_window_get_origin(gdk_window, &win_x, &win_y);

  GdkWindowEdge edge;
  gfx::Point point(static_cast<int>(event->x_root - win_x),
                   static_cast<int>(event->y_root - win_y));
  bool has_hit_edge = GetWindowEdge(point.x(), point.y(), &edge);
  if (has_hit_edge) {
    gdk_window_raise(gdk_window);
    EnsureDragHelperCreated();
    // Resize cursor was set by PanelGtk when mouse moved over window edge.
    GdkCursor* cursor =
        gdk_window_get_cursor(gtk_widget_get_window(GTK_WIDGET(window_)));
    drag_helper_->InitialWindowEdgeMousePress(event, cursor, edge);
    return TRUE;
  }

  return FALSE;  // Continue to propagate the event.
}

void PanelGtk::ActiveWindowChanged(GdkWindow* active_window) {
  // Do nothing if we're in the process of closing the panel window.
  if (!window_)
    return;

  bool is_active = gtk_widget_get_window(GTK_WIDGET(window_)) == active_window;
  if (is_active == is_active_)
    return;  // State did not change.

  if (is_active) {
    // If there's an app modal dialog (e.g., JS alert), try to redirect
    // the user's attention to the window owning the dialog.
    if (AppModalDialogQueue::GetInstance()->HasActiveDialog()) {
      AppModalDialogQueue::GetInstance()->ActivateModalDialog();
      return;
    }
  }

  is_active_ = is_active;
  titlebar_->UpdateTextColor();
  InvalidateWindow();
  panel_->OnActiveStateChanged(is_active_);
}

// Callback for the delete event.  This event is fired when the user tries to
// close the window.
gboolean PanelGtk::OnMainWindowDeleteEvent(GtkWidget* widget,
                                           GdkEvent* event) {
  ClosePanel();

  // Return true to prevent the gtk window from being destroyed.  Close will
  // destroy it for us.
  return TRUE;
}

void PanelGtk::OnMainWindowDestroy(GtkWidget* widget) {
  // BUG 8712. When we gtk_widget_destroy() in ClosePanel(), this will emit the
  // signal right away, and we will be here (while ClosePanel() is still in the
  // call stack). Let stack unwind before deleting the panel.
  //
  // We don't want to use DeleteSoon() here since it won't work on a nested pump
  // (like in UI tests).
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&base::DeletePointer<PanelGtk>, this));
}

void PanelGtk::ShowPanel() {
  gtk_window_present(window_);
  RevealPanel();
}

void PanelGtk::ShowPanelInactive() {
  gtk_window_set_focus_on_map(window_, false);
  gtk_widget_show(GTK_WIDGET(window_));
  RevealPanel();
}

void PanelGtk::RevealPanel() {
  DCHECK(!is_shown_);
  is_shown_ = true;
  SetBoundsInternal(bounds_);
}

gfx::Rect PanelGtk::GetPanelBounds() const {
  return bounds_;
}

void PanelGtk::SetPanelBounds(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds);
}

void PanelGtk::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds);
}

void PanelGtk::SetBoundsInternal(const gfx::Rect& bounds) {
  if (is_shown_) {
    gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window_)),
                           bounds.x(), bounds.y(),
                           bounds.width(), bounds.height());
  }

  bounds_ = bounds;

  titlebar_->SendEnterNotifyToCloseButtonIfUnderMouse();
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelGtk::ClosePanel() {
  // We're already closing.  Do nothing.
  if (!window_)
    return;

  if (!panel_->ShouldCloseWindow())
    return;

  if (drag_helper_.get())
    drag_helper_.reset();

  if (accel_group_)
    DisconnectAccelerators();

  // Cancel any pending callback from the loading animation timer.
  loading_animation_timer_.Stop();

  if (panel_->GetWebContents()) {
    // Hide the window (so it appears to have closed immediately).
    // When web contents are destroyed, we will be called back again.
    gtk_widget_hide(GTK_WIDGET(window_));
    panel_->OnWindowClosing();
    return;
  }

  GtkWidget* window = GTK_WIDGET(window_);
  // To help catch bugs in any event handlers that might get fired during the
  // destruction, set window_ to NULL before any handlers will run.
  window_ = NULL;

  panel_->OnNativePanelClosed();

  // We don't want GlobalMenuBar handling any notifications or commands after
  // the window is destroyed.
  // TODO(jennb):  global_menu_bar_->Disable();
  gtk_widget_destroy(window);
}

void PanelGtk::ActivatePanel() {
  gtk_window_present(window_);
}

void PanelGtk::DeactivatePanel() {
  gdk_window_lower(gtk_widget_get_window(GTK_WIDGET(window_)));

  // Per ICCCM: http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.7
  // A convention is also required for clients that want to give up the
  // input focus. There is no safe value set for them to set the input
  // focus to; therefore, they should ignore input material.
  //
  // No way to deactive a GTK window. Pretend panel is deactivated
  // and ignore input.
  ActiveWindowChanged(NULL);
}

bool PanelGtk::IsPanelActive() const {
  return is_active_;
}

void PanelGtk::PreventActivationByOS(bool prevent_activation) {
  gtk_window_set_accept_focus(window_, !prevent_activation);
}

gfx::NativeWindow PanelGtk::GetNativePanelHandle() {
  return window_;
}

void PanelGtk::UpdatePanelTitleBar() {
  TRACE_EVENT0("ui::gtk", "PanelGtk::UpdatePanelTitleBar");
  string16 title = panel_->GetWindowTitle();
  gtk_window_set_title(window_, UTF16ToUTF8(title).c_str());
  titlebar_->UpdateTitleAndIcon();

  gfx::Image app_icon = panel_->app_icon();
  if (!app_icon.IsEmpty())
    gtk_util::SetWindowIcon(window_, panel_->profile(), app_icon.ToGdkPixbuf());
}

void PanelGtk::UpdatePanelLoadingAnimations(bool should_animate) {
  if (should_animate) {
    if (!loading_animation_timer_.IsRunning()) {
      // Loads are happening, and the timer isn't running, so start it.
      loading_animation_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kLoadingAnimationFrameTimeMs),
          this,
          &PanelGtk::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      loading_animation_timer_.Stop();
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void PanelGtk::LoadingAnimationCallback() {
  titlebar_->UpdateThrobber(panel_->GetWebContents());
}

void PanelGtk::NotifyPanelOnUserChangedTheme() {
  titlebar_->UpdateTextColor();
  InvalidateWindow();
}

void PanelGtk::PanelWebContentsFocused(content::WebContents* contents) {
  // Nothing to do.
}

void PanelGtk::PanelCut() {
  gtk_window_util::DoCut(window_, panel_->GetWebContents());
}

void PanelGtk::PanelCopy() {
  gtk_window_util::DoCopy(window_, panel_->GetWebContents());
}

void PanelGtk::PanelPaste() {
  gtk_window_util::DoPaste(window_, panel_->GetWebContents());
}

void PanelGtk::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  if (is_drawing_attention_ == draw_attention)
    return;

  is_drawing_attention_ = draw_attention;

  titlebar_->UpdateTextColor();
  InvalidateWindow();

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0) {
    // May not be respected by all window managers.
    gtk_window_set_urgency_hint(window_, draw_attention);
  }
}

bool PanelGtk::IsDrawingAttention() const {
  return is_drawing_attention_;
}

void PanelGtk::HandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  GdkEventKey* os_event = &event.os_event->key;
  if (os_event && event.type == WebKit::WebInputEvent::RawKeyDown)
    gtk_window_activate_key(window_, os_event);
}

void PanelGtk::FullScreenModeChanged(bool is_full_screen) {
  // Nothing to do here as z-order rules for panels ensures that they're below
  // any app running in full screen mode.
}

void PanelGtk::PanelExpansionStateChanging(
    Panel::ExpansionState old_state, Panel::ExpansionState new_state) {
}

void PanelGtk::AttachWebContents(content::WebContents* contents) {
  if (!contents)
    return;
  gfx::NativeView widget = contents->GetNativeView();
  if (widget) {
    gtk_container_add(GTK_CONTAINER(contents_expanded_), widget);
    gtk_widget_show(widget);
    contents->WasShown();
  }
}

void PanelGtk::DetachWebContents(content::WebContents* contents) {
  gfx::NativeView widget = contents->GetNativeView();
  if (widget) {
    GtkWidget* parent = gtk_widget_get_parent(widget);
    if (parent) {
      DCHECK_EQ(parent, contents_expanded_);
      gtk_container_remove(GTK_CONTAINER(contents_expanded_), widget);
    }
  }
}

gfx::Size PanelGtk::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  gfx::Size& frame_size = GetFrameSize();
  return gfx::Size(content_size.width() + frame_size.width(),
                   content_size.height() + frame_size.height());
}

gfx::Size PanelGtk::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  gfx::Size& frame_size = GetFrameSize();
  return gfx::Size(window_size.width() - frame_size.width(),
                   window_size.height() - frame_size.height());
}

int PanelGtk::TitleOnlyHeight() const {
  gfx::Size& frame_size = GetFrameSize();
  if (!frame_size.IsEmpty())
    return panel::kTitlebarHeight;

  NOTREACHED() << "Checking title height before window allocated";
  return 0;
}

bool PanelGtk::IsPanelAlwaysOnTop() const {
  return always_on_top_;
}

void PanelGtk::SetPanelAlwaysOnTop(bool on_top) {
  if (always_on_top_ == on_top)
    return;
  always_on_top_ = on_top;

  gtk_window_set_keep_above(window_, on_top);

  // Do not show an icon in the task bar for always-on-top windows.
  // Window operations such as close, minimize etc. can only be done
  // from the panel UI.
  gtk_window_set_skip_taskbar_hint(window_, on_top);

  // Show always-on-top windows on all the virtual desktops.
  if (on_top)
    gtk_window_stick(window_);
  else
    gtk_window_unstick(window_);
}

void PanelGtk::EnableResizeByMouse(bool enable) {
}

void PanelGtk::UpdatePanelMinimizeRestoreButtonVisibility() {
  titlebar_->UpdateMinimizeRestoreButtonVisibility();
}

gfx::Size PanelGtk::GetNonClientFrameSize() const {
  GtkAllocation window_allocation;
  gtk_widget_get_allocation(window_container_, &window_allocation);
  GtkAllocation contents_allocation;
  gtk_widget_get_allocation(contents_expanded_, &contents_allocation);
  return gfx::Size(window_allocation.width - contents_allocation.width,
                   window_allocation.height - contents_allocation.height);
}

void PanelGtk::InvalidateWindow() {
  GtkAllocation allocation;
  gtk_widget_get_allocation(GTK_WIDGET(window_), &allocation);
  gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(window_)),
                             &allocation, TRUE);
}

// NativePanelTesting implementation.
class GtkNativePanelTesting : public NativePanelTesting {
 public:
  explicit GtkNativePanelTesting(PanelGtk* panel_gtk);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& mouse_location, panel::ClickModifier modifier) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar(
      panel::ClickModifier modifier) OVERRIDE;
  virtual void DragTitlebar(const gfx::Point& mouse_location) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;
  virtual bool VerifyDrawingAttention() const OVERRIDE;
  virtual bool VerifyActiveState(bool is_active) OVERRIDE;
  virtual bool VerifyAppIcon() const OVERRIDE;
  virtual bool VerifySystemMinimizeState() const OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;
  virtual bool IsButtonVisible(
      panel::TitlebarButtonType button_type) const OVERRIDE;

  PanelGtk* panel_gtk_;
};

NativePanelTesting* PanelGtk::CreateNativePanelTesting() {
  return new GtkNativePanelTesting(this);
}

GtkNativePanelTesting::GtkNativePanelTesting(PanelGtk* panel_gtk)
    : panel_gtk_(panel_gtk) {
}

void GtkNativePanelTesting::PressLeftMouseButtonTitlebar(
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {

  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = 1;
  event->button.x_root = mouse_location.x();
  event->button.y_root = mouse_location.y();
  if (modifier == panel::APPLY_TO_ALL)
    event->button.state |= GDK_CONTROL_MASK;
  panel_gtk_->OnTitlebarButtonPressEvent(
      NULL, reinterpret_cast<GdkEventButton*>(event));
  gdk_event_free(event);
  MessageLoopForUI::current()->RunUntilIdle();
}

void GtkNativePanelTesting::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_RELEASE);
  event->button.button = 1;
  if (modifier == panel::APPLY_TO_ALL)
    event->button.state |= GDK_CONTROL_MASK;
  if (panel_gtk_->drag_helper_.get()) {
    panel_gtk_->drag_helper_->OnButtonReleaseEvent(
        NULL, reinterpret_cast<GdkEventButton*>(event));
  } else {
    panel_gtk_->OnTitlebarButtonReleaseEvent(
        NULL, reinterpret_cast<GdkEventButton*>(event));
  }
  gdk_event_free(event);
  MessageLoopForUI::current()->RunUntilIdle();
}

void GtkNativePanelTesting::DragTitlebar(const gfx::Point& mouse_location) {
  if (!panel_gtk_->drag_helper_.get())
    return;
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
  event->motion.x_root = mouse_location.x();
  event->motion.y_root = mouse_location.y();
  panel_gtk_->drag_helper_->OnMouseMoveEvent(
      NULL, reinterpret_cast<GdkEventMotion*>(event));
  gdk_event_free(event);
  MessageLoopForUI::current()->RunUntilIdle();
}

void GtkNativePanelTesting::CancelDragTitlebar() {
  if (!panel_gtk_->drag_helper_.get())
    return;
  panel_gtk_->drag_helper_->OnGrabBrokenEvent(NULL, NULL);
  MessageLoopForUI::current()->RunUntilIdle();
}

void GtkNativePanelTesting::FinishDragTitlebar() {
  if (!panel_gtk_->drag_helper_.get())
    return;
  ReleaseMouseButtonTitlebar(panel::NO_MODIFIER);
}

bool GtkNativePanelTesting::VerifyDrawingAttention() const {
  return panel_gtk_->IsDrawingAttention();
}

bool GtkNativePanelTesting::VerifyActiveState(bool is_active) {
  // TODO(jianli): to be implemented. http://crbug.com/102737
  return false;
}

bool GtkNativePanelTesting::VerifyAppIcon() const {
  GdkPixbuf* icon = gtk_window_get_icon(panel_gtk_->GetNativePanelHandle());
  return icon &&
         gdk_pixbuf_get_width(icon) == panel::kPanelAppIconSize &&
         gdk_pixbuf_get_height(icon) == panel::kPanelAppIconSize;
}

bool GtkNativePanelTesting::VerifySystemMinimizeState() const {
  // TODO(jianli): to be implemented.
  return true;
}

bool GtkNativePanelTesting::IsWindowSizeKnown() const {
  return !GetFrameSize().IsEmpty();
}

bool GtkNativePanelTesting::IsAnimatingBounds() const {
  return false;
}

bool GtkNativePanelTesting::IsButtonVisible(
    panel::TitlebarButtonType button_type) const {
  PanelTitlebarGtk* titlebar = panel_gtk_->titlebar();
  CustomDrawButton* button;
  switch (button_type) {
    case panel::CLOSE_BUTTON:
      button = titlebar->close_button();
      break;
    case panel::MINIMIZE_BUTTON:
      button = titlebar->minimize_button();
      break;
    case panel::RESTORE_BUTTON:
      button = titlebar->restore_button();
      break;
    default:
      NOTREACHED();
      return false;
  }
  return gtk_widget_get_visible(button->widget());
}
