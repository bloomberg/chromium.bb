// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_window_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include <string>

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/base_paths_linux.h"
#include "base/command_line.h"
#include "base/gfx/gtk_util.h"
#include "base/gfx/rect.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/gtk/about_chrome_dialog.h"
#include "chrome/browser/gtk/active_window_watcher.h"
#include "chrome/browser/gtk/bookmark_bar_gtk.h"
#include "chrome/browser/gtk/bookmark_manager_gtk.h"
#include "chrome/browser/gtk/browser_titlebar.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/gtk/clear_browsing_data_dialog_gtk.h"
#include "chrome/browser/gtk/download_shelf_gtk.h"
#include "chrome/browser/gtk/edit_search_engine_dialog.h"
#include "chrome/browser/gtk/extension_shelf_gtk.h"
#include "chrome/browser/gtk/find_bar_gtk.h"
#include "chrome/browser/gtk/go_button_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/import_dialog_gtk.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"
#include "chrome/browser/gtk/keyword_editor_view.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/status_bubble_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/gtk/task_manager_gtk.h"
#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/views/compact_navigation_bar.h"
#include "chrome/browser/views/frame/status_area_view.h"
#include "chrome/browser/views/panel_controller.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "views/widget/widget_gtk.h"

#define COMPACT_NAV_BAR
#endif

namespace {

// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;

// Default offset of the contents splitter in pixels.
const int kDefaultContentsSplitOffset = 400;

const char* kBrowserWindowKey = "__BROWSER_WINDOW_GTK__";

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
// The thickness of the shadow around the toolbar+web content area.  There are
// actually a couple pixels more that should overlap the toolbar and web
// content area, but we don't use those pixels.
const int kContentShadowThickness = 2;

base::LazyInstance<ActiveWindowWatcher>
    g_active_window_watcher(base::LINKER_INITIALIZED);

gboolean MainWindowConfigured(GtkWindow* window, GdkEventConfigure* event,
                              BrowserWindowGtk* browser_win) {
  gfx::Rect bounds = gfx::Rect(event->x, event->y, event->width, event->height);
  browser_win->OnBoundsChanged(bounds);
  return FALSE;
}

gboolean MainWindowStateChanged(GtkWindow* window, GdkEventWindowState* event,
                                BrowserWindowGtk* browser_win) {
  browser_win->OnStateChanged(event->new_window_state);
  return FALSE;
}

// Callback for the delete event.  This event is fired when the user tries to
// close the window (e.g., clicking on the X in the window manager title bar).
gboolean MainWindowDeleteEvent(GtkWidget* widget, GdkEvent* event,
                               BrowserWindowGtk* window) {
  window->Close();

  // Return true to prevent the gtk window from being destroyed.  Close will
  // destroy it for us.
  return TRUE;
}

void MainWindowDestroy(GtkWidget* widget, BrowserWindowGtk* window) {
  // BUG 8712. When we gtk_widget_destroy() in Close(), this will emit the
  // signal right away, and we will be here (while Close() is still in the
  // call stack).  In order to not reenter Close(), and to also follow the
  // expectations of BrowserList, we should run the BrowserWindowGtk destructor
  // not now, but after the run loop goes back to process messages.  Otherwise
  // we will remove ourself from BrowserList while it's being iterated.
  // Additionally, now that we know the window is gone, we need to make sure to
  // set window_ to NULL, otherwise we will try to close the window again when
  // we call Close() in the destructor.
  //
  // We don't want to use DeleteSoon() here since it won't work on a nested pump
  // (like in UI tests).
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new DeleteTask<BrowserWindowGtk>(window));
}

// Using gtk_window_get_position/size creates a race condition, so only use
// this to get the initial bounds.  After window creation, we pick up the
// normal bounds by connecting to the configure-event signal.
gfx::Rect GetInitialWindowBounds(GtkWindow* window) {
  gint x, y, width, height;
  gtk_window_get_position(window, &x, &y);
  gtk_window_get_size(window, &width, &height);
  return gfx::Rect(x, y, width, height);
}

// Keep this in sync with various context menus which display the accelerators.
const struct AcceleratorMapping {
  guint keyval;
  int command_id;
  GdkModifierType modifier_type;
} kAcceleratorMap[] = {
  // Focus.
  { GDK_k, IDC_FOCUS_SEARCH, GDK_CONTROL_MASK },
  { GDK_e, IDC_FOCUS_SEARCH, GDK_CONTROL_MASK },
  { XF86XK_Search, IDC_FOCUS_SEARCH, GdkModifierType(0) },
  { GDK_l, IDC_FOCUS_LOCATION, GDK_CONTROL_MASK },
  { GDK_d, IDC_FOCUS_LOCATION, GDK_MOD1_MASK },
  { GDK_F6, IDC_FOCUS_LOCATION, GdkModifierType(0) },
  { XF86XK_OpenURL, IDC_FOCUS_LOCATION, GdkModifierType(0) },
  { XF86XK_Go, IDC_FOCUS_LOCATION, GdkModifierType(0) },

  // Tab/window controls.
  { GDK_Page_Down, IDC_SELECT_NEXT_TAB, GDK_CONTROL_MASK },
  { GDK_Page_Up, IDC_SELECT_PREVIOUS_TAB, GDK_CONTROL_MASK },
  { GDK_w, IDC_CLOSE_TAB, GDK_CONTROL_MASK },
  { GDK_t, IDC_RESTORE_TAB,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },

  { GDK_1, IDC_SELECT_TAB_0, GDK_CONTROL_MASK },
  { GDK_2, IDC_SELECT_TAB_1, GDK_CONTROL_MASK },
  { GDK_3, IDC_SELECT_TAB_2, GDK_CONTROL_MASK },
  { GDK_4, IDC_SELECT_TAB_3, GDK_CONTROL_MASK },
  { GDK_5, IDC_SELECT_TAB_4, GDK_CONTROL_MASK },
  { GDK_6, IDC_SELECT_TAB_5, GDK_CONTROL_MASK },
  { GDK_7, IDC_SELECT_TAB_6, GDK_CONTROL_MASK },
  { GDK_8, IDC_SELECT_TAB_7, GDK_CONTROL_MASK },
  { GDK_9, IDC_SELECT_LAST_TAB, GDK_CONTROL_MASK },

  { GDK_1, IDC_SELECT_TAB_0, GDK_MOD1_MASK },
  { GDK_2, IDC_SELECT_TAB_1, GDK_MOD1_MASK },
  { GDK_3, IDC_SELECT_TAB_2, GDK_MOD1_MASK },
  { GDK_4, IDC_SELECT_TAB_3, GDK_MOD1_MASK },
  { GDK_5, IDC_SELECT_TAB_4, GDK_MOD1_MASK },
  { GDK_6, IDC_SELECT_TAB_5, GDK_MOD1_MASK },
  { GDK_7, IDC_SELECT_TAB_6, GDK_MOD1_MASK },
  { GDK_8, IDC_SELECT_TAB_7, GDK_MOD1_MASK },
  { GDK_9, IDC_SELECT_LAST_TAB, GDK_MOD1_MASK },

  { GDK_KP_1, IDC_SELECT_TAB_0, GDK_CONTROL_MASK },
  { GDK_KP_2, IDC_SELECT_TAB_1, GDK_CONTROL_MASK },
  { GDK_KP_3, IDC_SELECT_TAB_2, GDK_CONTROL_MASK },
  { GDK_KP_4, IDC_SELECT_TAB_3, GDK_CONTROL_MASK },
  { GDK_KP_5, IDC_SELECT_TAB_4, GDK_CONTROL_MASK },
  { GDK_KP_6, IDC_SELECT_TAB_5, GDK_CONTROL_MASK },
  { GDK_KP_7, IDC_SELECT_TAB_6, GDK_CONTROL_MASK },
  { GDK_KP_8, IDC_SELECT_TAB_7, GDK_CONTROL_MASK },
  { GDK_KP_9, IDC_SELECT_LAST_TAB, GDK_CONTROL_MASK },

  { GDK_KP_1, IDC_SELECT_TAB_0, GDK_MOD1_MASK },
  { GDK_KP_2, IDC_SELECT_TAB_1, GDK_MOD1_MASK },
  { GDK_KP_3, IDC_SELECT_TAB_2, GDK_MOD1_MASK },
  { GDK_KP_4, IDC_SELECT_TAB_3, GDK_MOD1_MASK },
  { GDK_KP_5, IDC_SELECT_TAB_4, GDK_MOD1_MASK },
  { GDK_KP_6, IDC_SELECT_TAB_5, GDK_MOD1_MASK },
  { GDK_KP_7, IDC_SELECT_TAB_6, GDK_MOD1_MASK },
  { GDK_KP_8, IDC_SELECT_TAB_7, GDK_MOD1_MASK },
  { GDK_KP_9, IDC_SELECT_LAST_TAB, GDK_MOD1_MASK },

  { GDK_F4, IDC_CLOSE_TAB, GDK_CONTROL_MASK },
  { GDK_F4, IDC_CLOSE_WINDOW, GDK_MOD1_MASK },

  // Zoom level.
  { GDK_plus, IDC_ZOOM_PLUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_equal, IDC_ZOOM_PLUS, GDK_CONTROL_MASK },
  { XF86XK_ZoomIn, IDC_ZOOM_PLUS, GdkModifierType(0) },
  { GDK_0, IDC_ZOOM_NORMAL, GDK_CONTROL_MASK },
  { GDK_minus, IDC_ZOOM_MINUS, GDK_CONTROL_MASK },
  { GDK_underscore, IDC_ZOOM_MINUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { XF86XK_ZoomOut, IDC_ZOOM_MINUS, GdkModifierType(0) },

  // Find in page.
  { GDK_g, IDC_FIND_NEXT, GDK_CONTROL_MASK },
  { GDK_F3, IDC_FIND_NEXT, GdkModifierType(0) },
  { GDK_g, IDC_FIND_PREVIOUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_F3, IDC_FIND_PREVIOUS, GDK_SHIFT_MASK },

  // Navigation / toolbar buttons.
  { GDK_Home, IDC_HOME, GDK_MOD1_MASK },
  { XF86XK_HomePage, IDC_HOME, GdkModifierType(0) },
  { GDK_Escape, IDC_STOP, GdkModifierType(0) },
  { XF86XK_Stop, IDC_STOP, GdkModifierType(0) },
  { GDK_Left, IDC_BACK, GDK_MOD1_MASK },
  { GDK_BackSpace, IDC_BACK, GdkModifierType(0) },
  { XF86XK_Back, IDC_BACK, GdkModifierType(0) },
  { GDK_Right, IDC_FORWARD, GDK_MOD1_MASK },
  { GDK_BackSpace, IDC_FORWARD, GDK_SHIFT_MASK },
  { XF86XK_Forward, IDC_FORWARD, GdkModifierType(0) },
  { GDK_r, IDC_RELOAD, GDK_CONTROL_MASK },
  { GDK_F5, IDC_RELOAD, GdkModifierType(0) },
  { GDK_F5, IDC_RELOAD, GDK_CONTROL_MASK },
  { GDK_F5, IDC_RELOAD, GDK_SHIFT_MASK },
  { XF86XK_Reload, IDC_RELOAD, GdkModifierType(0) },
  { XF86XK_Refresh, IDC_RELOAD, GdkModifierType(0) },

  // Miscellany.
  { GDK_d, IDC_STAR, GDK_CONTROL_MASK },
  { XF86XK_AddFavorite, IDC_STAR, GdkModifierType(0) },
  { XF86XK_Favorites, IDC_SHOW_BOOKMARK_BAR, GdkModifierType(0) },
  { XF86XK_History, IDC_SHOW_HISTORY, GdkModifierType(0) },
  { GDK_o, IDC_OPEN_FILE, GDK_CONTROL_MASK },
  { GDK_F11, IDC_FULLSCREEN, GdkModifierType(0) },
  { GDK_u, IDC_VIEW_SOURCE, GDK_CONTROL_MASK },
  { GDK_p, IDC_PRINT, GDK_CONTROL_MASK },
  { GDK_Escape, IDC_TASK_MANAGER, GDK_SHIFT_MASK },

#if defined(OS_CHROMEOS)
  { GDK_f, IDC_FULLSCREEN,
    GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK) },
  { GDK_Delete, IDC_TASK_MANAGER,
    GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK) },
  { GDK_comma, IDC_CONTROL_PANEL, GdkModifierType(GDK_CONTROL_MASK) },
#endif
};

int GetCommandId(guint accel_key, GdkModifierType modifier) {
  // Bug 9806: If capslock is on, we will get a capital letter as accel_key.
  accel_key = gdk_keyval_to_lower(accel_key);
  // Filter modifier to only include accelerator modifiers.
  modifier = static_cast<GdkModifierType>(
      modifier & gtk_accelerator_get_default_mod_mask());
  for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
    if (kAcceleratorMap[i].keyval == accel_key &&
        kAcceleratorMap[i].modifier_type == modifier)
      return kAcceleratorMap[i].command_id;
  }
  NOTREACHED();
  return 0;
}

// An event handler for key press events.  We need to special case key
// combinations that are not valid gtk accelerators.  This function returns
// TRUE if it can handle the key press.
gboolean HandleCustomAccelerator(guint keyval, GdkModifierType modifier,
                                 Browser* browser) {
  // Filter modifier to only include accelerator modifiers.
  modifier = static_cast<GdkModifierType>(
      modifier & gtk_accelerator_get_default_mod_mask());
  switch (keyval) {
    // Gtk doesn't allow GDK_Tab or GDK_ISO_Left_Tab to be an accelerator (see
    // gtk_accelerator_valid), so we need to handle these accelerators
    // manually.
    // Some X clients (e.g. cygwin, NX client, etc.) also send GDK_KP_Tab when
    // typing a tab key. We should also handle GDK_KP_Tab for such X clients as
    // Firefox does.
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
    case GDK_KP_Tab:
      if (GDK_CONTROL_MASK == modifier) {
        browser->ExecuteCommand(IDC_SELECT_NEXT_TAB);
        return TRUE;
      } else if ((GDK_CONTROL_MASK | GDK_SHIFT_MASK) == modifier) {
        browser->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
        return TRUE;
      }
      break;

    default:
      break;
  }
  return FALSE;
}

// Handle accelerators that we don't want the native widget to be able to
// override.
gboolean PreHandleAccelerator(guint keyval, GdkModifierType modifier,
                              Browser* browser) {
  // Filter modifier to only include accelerator modifiers.
  modifier = static_cast<GdkModifierType>(
      modifier & gtk_accelerator_get_default_mod_mask());
  switch (keyval) {
    case GDK_Page_Down:
      if (GDK_CONTROL_MASK == modifier) {
        browser->ExecuteCommand(IDC_SELECT_NEXT_TAB);
        return TRUE;
      }
      break;

    case GDK_Page_Up:
      if (GDK_CONTROL_MASK == modifier) {
        browser->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
        return TRUE;
      }
      break;

    default:
      break;
  }
  return FALSE;
}

// Let the focused widget have first crack at the key event so we don't
// override their accelerators.
gboolean OnKeyPress(GtkWindow* window, GdkEventKey* event, Browser* browser) {
  // If a widget besides the native view is focused, we have to try to handle
  // the custom accelerators before letting it handle them.
  TabContents* current_tab_contents =
      browser->tabstrip_model()->GetSelectedTabContents();
  // The current tab might not have a render view if it crashed.
  if (!current_tab_contents || !current_tab_contents->GetContentNativeView() ||
      !gtk_widget_is_focus(current_tab_contents->GetContentNativeView())) {
    if (HandleCustomAccelerator(event->keyval,
        GdkModifierType(event->state), browser) ||
        PreHandleAccelerator(event->keyval,
        GdkModifierType(event->state), browser)) {
      return TRUE;
    }

    return gtk_window_propagate_key_event(window, event);
  } else {
    bool rv = gtk_window_propagate_key_event(window, event);
    DCHECK(rv);
    return TRUE;
  }
}

GdkCursorType GdkWindowEdgeToGdkCursorType(GdkWindowEdge edge) {
  switch (edge) {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      return GDK_TOP_LEFT_CORNER;
    case GDK_WINDOW_EDGE_NORTH:
      return GDK_TOP_SIDE;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      return GDK_TOP_RIGHT_CORNER;
    case GDK_WINDOW_EDGE_WEST:
      return GDK_LEFT_SIDE;
    case GDK_WINDOW_EDGE_EAST:
      return GDK_RIGHT_SIDE;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      return GDK_BOTTOM_LEFT_CORNER;
    case GDK_WINDOW_EDGE_SOUTH:
      return GDK_BOTTOM_SIDE;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      return GDK_BOTTOM_RIGHT_CORNER;
    default:
      NOTREACHED();
  }
  return GDK_LAST_CURSOR;
}

}  // namespace

std::map<XID, GtkWindow*> BrowserWindowGtk::xid_map_;

BrowserWindowGtk::BrowserWindowGtk(Browser* browser)
    :  browser_(browser),
#if defined(OS_CHROMEOS)
       drag_active_(false),
       panel_controller_(NULL),
       compact_navigation_bar_(NULL),
       status_area_(NULL),
#endif
       frame_cursor_(NULL),
       is_active_(true),
       last_click_time_(0),
       maximize_after_show_(false),
       accel_group_(NULL) {
  use_custom_frame_.Init(prefs::kUseCustomChromeFrame,
      browser_->profile()->GetPrefs(), this);

  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  g_object_set_data(G_OBJECT(window_), kBrowserWindowKey, this);
  gtk_widget_add_events(GTK_WIDGET(window_), GDK_BUTTON_PRESS_MASK |
                                             GDK_POINTER_MOTION_MASK);

  // Add this window to its own unique window group to allow for
  // window-to-parent modality.
  gtk_window_group_add_window(gtk_window_group_new(), window_);
  g_object_unref(gtk_window_get_group(window_));

  gtk_util::SetWindowIcon(window_);
  SetBackgroundColor();
  SetGeometryHints();
  ConnectHandlersToSignals();
  ConnectAccelerators();
  bounds_ = GetInitialWindowBounds(window_);

  InitWidgets();
  HideUnsupportedWindowFeatures();

  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::ACTIVE_WINDOW_CHANGED,
                 NotificationService::AllSources());
  // Make sure the ActiveWindowWatcher instance exists (it's a lazy instance).
  g_active_window_watcher.Get();
}

BrowserWindowGtk::~BrowserWindowGtk() {
  browser_->tabstrip_model()->RemoveObserver(this);

  if (frame_cursor_) {
    gdk_cursor_unref(frame_cursor_);
    frame_cursor_ = NULL;
  }
}

void BrowserWindowGtk::HandleAccelerator(guint keyval,
                                         GdkModifierType modifier) {
  if (!HandleCustomAccelerator(keyval, modifier, browser_.get())) {
    // Pass the accelerator on to GTK.
    gtk_accel_groups_activate(G_OBJECT(window_), keyval, modifier);
  }
}

gboolean BrowserWindowGtk::OnCustomFrameExpose(GtkWidget* widget,
                                               GdkEventExpose* event,
                                               BrowserWindowGtk* window) {
  static NineBox* default_background = NULL;
  static NineBox* default_background_inactive = NULL;
  static NineBox* default_background_otr = NULL;
  static NineBox* default_background_otr_inactive = NULL;

  ThemeProvider* theme_provider =
      window->browser()->profile()->GetThemeProvider();
  if (!default_background) {
    default_background = new NineBox(theme_provider,
        0, IDR_THEME_FRAME, 0, 0, 0, 0, 0, 0, 0);
    default_background_inactive = new NineBox(theme_provider,
        0, IDR_THEME_FRAME_INACTIVE, 0, 0, 0, 0, 0, 0, 0);
    default_background_otr = new NineBox(theme_provider,
        0, IDR_THEME_FRAME_INCOGNITO, 0, 0, 0, 0, 0, 0, 0);
    default_background_otr_inactive = new NineBox(theme_provider,
        0, IDR_THEME_FRAME_INCOGNITO_INACTIVE, 0, 0, 0, 0, 0, 0, 0);
  }

  // Draw the default background.
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y, event->area.width,
                  event->area.height);
  cairo_clip(cr);
  NineBox* image = NULL;
  if (window->IsActive()) {
    image = window->browser()->profile()->IsOffTheRecord()
        ? default_background_otr : default_background;
  } else {
    image = window->browser()->profile()->IsOffTheRecord()
        ? default_background_otr_inactive : default_background_inactive;
  }
  image->RenderTopCenterStrip(cr, 0, 0, widget->allocation.width);

  if (theme_provider->HasCustomImage(IDR_THEME_FRAME_OVERLAY)) {
    GdkPixbuf* theme_overlay = theme_provider->GetPixbufNamed(
        window->IsActive() ? IDR_THEME_FRAME_OVERLAY
                           : IDR_THEME_FRAME_OVERLAY_INACTIVE);
    gdk_cairo_set_source_pixbuf(cr, theme_overlay, 0, 0);
    cairo_paint(cr);
  }

  DrawContentShadow(cr, window);

  cairo_destroy(cr);

  if (window->use_custom_frame_.GetValue() && !window->IsMaximized()) {
    static NineBox custom_frame_border(
          theme_provider,
          IDR_WINDOW_TOP_LEFT_CORNER,
          IDR_WINDOW_TOP_CENTER,
          IDR_WINDOW_TOP_RIGHT_CORNER,
          IDR_WINDOW_LEFT_SIDE,
          NULL,
          IDR_WINDOW_RIGHT_SIDE,
          IDR_WINDOW_BOTTOM_LEFT_CORNER,
          IDR_WINDOW_BOTTOM_CENTER,
          IDR_WINDOW_BOTTOM_RIGHT_CORNER);

    custom_frame_border.RenderToWidget(widget);
  }

  return FALSE;  // Allow subwidgets to paint.
}

// static
void BrowserWindowGtk::DrawContentShadow(cairo_t* cr,
                                         BrowserWindowGtk* window) {
  // Draw the shadow above the toolbar. Tabs on the tabstrip will draw over us.
  ThemeProvider* theme_provider =
      window->browser()->profile()->GetThemeProvider();
  static NineBox top_shadow(theme_provider,
                            0, IDR_CONTENT_TOP_CENTER, 0, 0, 0, 0, 0, 0, 0);
  int left_x, top_y;
  gtk_widget_translate_coordinates(window->content_vbox_,
      GTK_WIDGET(window->window_), 0, 0, &left_x,
      &top_y);
  int width = window->content_vbox_->allocation.width;
  top_shadow.RenderTopCenterStrip(cr,
      left_x, top_y - kContentShadowThickness, width);

  // Only draw the rest of the shadow if the user has the custom frame enabled.
  if (!window->use_custom_frame_.GetValue())
    return;

  // The top left corner has a width of 3 pixels. On Windows, the last column
  // of pixels overlap the toolbar. We just crop it off on Linux.  The top
  // corners extend to the base of the toolbar (one pixel above the dividing
  // line).
  int right_x = left_x + width;
  GdkPixbuf* top_left =
      theme_provider->GetPixbufNamed(IDR_CONTENT_TOP_LEFT_CORNER);
  gdk_cairo_set_source_pixbuf(cr, top_left,
      left_x - kContentShadowThickness, top_y - kContentShadowThickness);
  // The toolbar is shorter in location bar only mode so clip the image to the
  // height of the toolbar + the amount of shadow above the toolbar.
  int top_corner_height =
      window->toolbar_->widget()->allocation.height + kContentShadowThickness;
  cairo_rectangle(cr,
      left_x - kContentShadowThickness,
      top_y - kContentShadowThickness,
      kContentShadowThickness,
      top_corner_height);
  cairo_fill(cr);

  // Likewise, we crop off the left column of pixels for the top right corner.
  GdkPixbuf* top_right =
      theme_provider->GetPixbufNamed(IDR_CONTENT_TOP_RIGHT_CORNER);
  gdk_cairo_set_source_pixbuf(cr, top_right,
                              right_x - 1, top_y - kContentShadowThickness);
  cairo_rectangle(cr,
      right_x,
      top_y - kContentShadowThickness,
      kContentShadowThickness,
      top_corner_height);
  cairo_fill(cr);

  // Fill in the sides.  As above, we only draw 2 of the 3 columns on Linux.
  int height = window->content_vbox_->allocation.height;
  int bottom_y = top_y + height;
  // |side_y| is where to start drawing the side shadows.  The top corners draw
  // the sides down to the bottom of the toolbar.
  int side_y = top_y - kContentShadowThickness + top_corner_height;
  // |side_height| is how many pixels to draw for the side borders.  We do one
  // pixel before the bottom of the web contents because that extra pixel is
  // drawn by the bottom corners.
  int side_height = bottom_y - side_y - 1;
  if (side_height > 0) {
    GdkPixbuf* left = theme_provider->GetPixbufNamed(IDR_CONTENT_LEFT_SIDE);
    gdk_cairo_set_source_pixbuf(cr, left,
                                left_x - kContentShadowThickness, side_y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr,
        left_x - kContentShadowThickness,
        side_y,
        kContentShadowThickness,
        side_height);
    cairo_fill(cr);

    GdkPixbuf* right = theme_provider->GetPixbufNamed(IDR_CONTENT_RIGHT_SIDE);
    gdk_cairo_set_source_pixbuf(cr, right, right_x - 1, side_y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr,
        right_x,
        side_y,
        kContentShadowThickness,
        side_height);
    cairo_fill(cr);
  }

  // Draw the bottom corners.  The bottom corners also draw the bottom row of
  // pixels of the side shadows.
  GdkPixbuf* bottom_left =
      theme_provider->GetPixbufNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  gdk_cairo_set_source_pixbuf(cr, bottom_left,
                              left_x - kContentShadowThickness, bottom_y - 1);
  cairo_paint(cr);

  GdkPixbuf* bottom_right =
      theme_provider->GetPixbufNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER);
  gdk_cairo_set_source_pixbuf(cr, bottom_right,
                              right_x - 1, bottom_y - 1);
  cairo_paint(cr);

  // Finally, draw the bottom row. Since we don't overlap the contents, we clip
  // the top row of pixels.
  GdkPixbuf* bottom =
      theme_provider->GetPixbufNamed(IDR_CONTENT_BOTTOM_CENTER);
  gdk_cairo_set_source_pixbuf(cr, bottom,
                              left_x + 1, bottom_y - 1);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr,
      left_x + 1,
      bottom_y,
      width - 2,
      kContentShadowThickness);
  cairo_fill(cr);
}

void BrowserWindowGtk::Show() {
  // The Browser associated with this browser window must become the active
  // browser at the time Show() is called. This is the natural behaviour under
  // Windows, but gtk_widget_show won't show the widget (and therefore won't
  // call OnFocusIn()) until we return to the runloop. Therefore any calls to
  // BrowserList::GetLastActive() (for example, in bookmark_util), will return
  // the previous browser instead if we don't explicitly set it here.
  BrowserList::SetLastActive(browser());

#if defined(OS_CHROMEOS)
  if (browser_->type() == Browser::TYPE_POPUP) {
    panel_controller_ = new PanelController(this);
  } else {
    TabOverviewTypes::instance()->SetWindowType(
        GTK_WIDGET(window_),
        TabOverviewTypes::WINDOW_TYPE_CHROME_TOPLEVEL,
        NULL);
  }
#endif

  gtk_window_present(window_);
  if (maximize_after_show_) {
    gtk_window_maximize(window_);
    maximize_after_show_ = false;
  }
}

void BrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  gint x = static_cast<gint>(bounds.x());
  gint y = static_cast<gint>(bounds.y());
  gint width = static_cast<gint>(bounds.width());
  gint height = static_cast<gint>(bounds.height());

  gtk_window_move(window_, x, y);
  gtk_window_resize(window_, width, height);
}

void BrowserWindowGtk::Close() {
  // We're already closing.  Do nothing.
  if (!window_)
    return;

  if (!CanClose())
    return;

  SaveWindowPosition();

  if (accel_group_) {
    // Disconnecting the keys we connected to our accelerator group frees the
    // closures allocated in ConnectAccelerators.
    for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
      gtk_accel_group_disconnect_key(accel_group_,
                                     kAcceleratorMap[i].keyval,
                                     kAcceleratorMap[i].modifier_type);
    }
    gtk_window_remove_accel_group(window_, accel_group_);
    g_object_unref(accel_group_);
    accel_group_ = NULL;
  }

  GtkWidget* window = GTK_WIDGET(window_);
  // To help catch bugs in any event handlers that might get fired during the
  // destruction, set window_ to NULL before any handlers will run.
  window_ = NULL;
  gtk_widget_destroy(window);

#if defined(OS_CHROMEOS)
  if (panel_controller_) {
    panel_controller_->Close();
  }
#endif
}

void BrowserWindowGtk::Activate() {
  gtk_window_present(window_);
}

bool BrowserWindowGtk::IsActive() const {
  return is_active_;
}

void BrowserWindowGtk::FlashFrame() {
  // May not be respected by all window managers.
  gtk_window_set_urgency_hint(window_, TRUE);
}

gfx::NativeWindow BrowserWindowGtk::GetNativeHandle() {
  return window_;
}

BrowserWindowTesting* BrowserWindowGtk::GetBrowserWindowTesting() {
  NOTIMPLEMENTED();
  return NULL;
}

StatusBubble* BrowserWindowGtk::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserWindowGtk::SelectedTabToolbarSizeChanged(bool is_animating) {
  // On Windows, this is used for a performance optimization.
  // http://code.google.com/p/chromium/issues/detail?id=12291
}

void BrowserWindowGtk::UpdateTitleBar() {
#if defined(OS_CHROMEOS)
  if (panel_controller_)
    panel_controller_->UpdateTitleBar();
#endif

  string16 title = browser_->GetWindowTitleForCurrentTab();
  gtk_window_set_title(window_, UTF16ToUTF8(title).c_str());
  if (ShouldShowWindowIcon())
    titlebar_->UpdateTitle();

  // We need to update the bookmark bar state if we're navigating away from the
  // NTP and "always show bookmark bar" is not set.  On Windows,
  // UpdateTitleBar() causes a layout in BrowserView which checks to see if
  // the bookmarks bar should be shown.
  MaybeShowBookmarkBar(browser_->GetSelectedTabContents(), false);
}

void BrowserWindowGtk::UpdateDevTools() {
  UpdateDevToolsForContents(
      browser_->tabstrip_model()->GetSelectedTabContents());
}

void BrowserWindowGtk::FocusDevTools() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateLoadingAnimations(bool should_animate) {
  if (should_animate) {
    if (!loading_animation_timer_.IsRunning()) {
      // Loads are happening, and the timer isn't running, so start it.
      loading_animation_timer_.Start(
          base::TimeDelta::FromMilliseconds(kLoadingAnimationFrameTimeMs), this,
          &BrowserWindowGtk::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      loading_animation_timer_.Stop();
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void BrowserWindowGtk::LoadingAnimationCallback() {
  if (browser_->type() == Browser::TYPE_NORMAL) {
    // Loading animations are shown in the tab for tabbed windows.  We check the
    // browser type instead of calling IsTabStripVisible() because the latter
    // will return false for fullscreen windows, but we still need to update
    // their animations (so that when they come out of fullscreen mode they'll
    // be correct).
    tabstrip_->UpdateLoadingAnimations();
  } else if (ShouldShowWindowIcon()) {
    // ... or in the window icon area for popups and app windows.
    TabContents* tab_contents = browser_->GetSelectedTabContents();
    // GetSelectedTabContents can return NULL for example under Purify when
    // the animations are running slowly and this function is called on
    // a timer through LoadingAnimationCallback.
    titlebar_->UpdateThrobber(tab_contents);
  }
}

void BrowserWindowGtk::SetStarredState(bool is_starred) {
  toolbar_->star()->SetStarred(is_starred);
}

gfx::Rect BrowserWindowGtk::GetRestoredBounds() const {
  return restored_bounds_;
}

bool BrowserWindowGtk::IsMaximized() const {
  return (state_ & GDK_WINDOW_STATE_MAXIMIZED);
}

void BrowserWindowGtk::SetFullscreen(bool fullscreen) {
  // gtk_window_(un)fullscreen asks the window manager to toggle the EWMH
  // for fullscreen windows.  Not all window managers support this.
  if (fullscreen) {
    gtk_window_fullscreen(window_);
  } else {
    gtk_window_unfullscreen(window_);
  }
}

bool BrowserWindowGtk::IsFullscreen() const {
  return (state_ & GDK_WINDOW_STATE_FULLSCREEN);
}

LocationBar* BrowserWindowGtk::GetLocationBar() const {
  return toolbar_->GetLocationBar();
}

void BrowserWindowGtk::SetFocusToLocationBar() {
  if (!IsFullscreen())
    GetLocationBar()->FocusLocation();
}

void BrowserWindowGtk::UpdateStopGoState(bool is_loading, bool force) {
  toolbar_->GetGoButton()->ChangeMode(
      is_loading ? GoButtonGtk::MODE_STOP : GoButtonGtk::MODE_GO, force);
}

void BrowserWindowGtk::UpdateToolbar(TabContents* contents,
                                     bool should_restore_state) {
  toolbar_->UpdateTabContents(contents, should_restore_state);
}

void BrowserWindowGtk::FocusToolbar() {
  NOTIMPLEMENTED();
}

bool BrowserWindowGtk::IsBookmarkBarVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR) &&
      bookmark_bar_.get();
}

gfx::Rect BrowserWindowGtk::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

void BrowserWindowGtk::ConfirmAddSearchProvider(const TemplateURL* template_url,
                                                Profile* profile) {
  new EditSearchEngineDialog(window_, template_url, NULL, profile);
}

void BrowserWindowGtk::ToggleBookmarkBar() {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

void BrowserWindowGtk::ShowAboutChromeDialog() {
  ShowAboutDialogForProfile(window_, browser_->profile());
}

void BrowserWindowGtk::ShowTaskManager() {
  TaskManagerGtk::Show();
}

void BrowserWindowGtk::ShowBookmarkManager() {
  BookmarkManagerGtk::Show(browser_->profile());
}

void BrowserWindowGtk::ShowBookmarkBubble(const GURL& url,
                                          bool already_bookmarked) {
  toolbar_->star()->ShowStarBubble(url, !already_bookmarked);
}

bool BrowserWindowGtk::IsDownloadShelfVisible() const {
  return download_shelf_.get() && download_shelf_->IsShowing();
}

DownloadShelf* BrowserWindowGtk::GetDownloadShelf() {
  if (!download_shelf_.get())
    download_shelf_.reset(new DownloadShelfGtk(browser_.get(),
                                               render_area_vbox_));
  return download_shelf_.get();
}

void BrowserWindowGtk::ShowReportBugDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowClearBrowsingDataDialog() {
  ClearBrowsingDataDialogGtk::Show(window_, browser_->profile());
}

void BrowserWindowGtk::ShowImportDialog() {
  ImportDialogGtk::Show(window_, browser_->profile());
}

void BrowserWindowGtk::ShowSearchEnginesDialog() {
  KeywordEditorView::Show(browser_->profile());
}

void BrowserWindowGtk::ShowPasswordManager() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowSelectProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowNewProfileDialog() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                      gfx::NativeWindow parent_window) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UserChangedTheme() {
  SetBackgroundColor();
  gdk_window_invalidate_rect(GTK_WIDGET(window_)->window,
      &GTK_WIDGET(window_)->allocation, TRUE);
  UpdateWindowShape(bounds_.width(), bounds_.height());
}

int BrowserWindowGtk::GetExtraRenderViewHeight() const {
  int sum = infobar_container_->TotalHeightOfClosingBars();
  if (bookmark_bar_->IsClosing())
    sum += bookmark_bar_->GetHeight();
  if (download_shelf_.get() && download_shelf_->IsClosing()) {
    sum += download_shelf_->GetHeight();
  }
  return sum;
}

void BrowserWindowGtk::TabContentsFocused(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowPageInfo(Profile* profile,
                                    const GURL& url,
                                    const NavigationEntry::SSLStatus& ssl,
                                    bool show_history) {
  browser::ShowPageInfo(window_, profile, url, ssl, show_history);
}

void BrowserWindowGtk::ConfirmBrowserCloseWithPendingDownloads() {
  NOTIMPLEMENTED();
  browser_->InProgressDownloadResponse(false);
}

void BrowserWindowGtk::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED:
      MaybeShowBookmarkBar(browser_->GetSelectedTabContents(), true);
      break;

    case NotificationType::PREF_CHANGED: {
      std::wstring* pref_name = Details<std::wstring>(details).ptr();
      if (*pref_name == prefs::kUseCustomChromeFrame) {
        UpdateCustomFrame();
      } else {
        NOTREACHED() << "Got pref change notification we didn't register for!";
      }
      break;
    }

    case NotificationType::ACTIVE_WINDOW_CHANGED: {
      // Do nothing if we're in the process of closing the browser window.
      if (!window_)
        break;

      // If there's an app modal dialog (e.g., JS alert), try to redirect
      // the user's attention to the window owning the dialog.
      if (Singleton<AppModalDialogQueue>()->HasActiveDialog()) {
        Singleton<AppModalDialogQueue>()->ActivateModalDialog();
        break;
      }

      // If we lose focus to an info bubble, we don't want to seem inactive.
      // However we can only control this when we are painting a custom
      // frame. So if we lose focus BUT it's to one of our info bubbles AND we
      // are painting a custom frame, then paint as if we are active.
      const GdkWindow* active_window = Details<const GdkWindow>(details).ptr();
      const GtkWindow* info_bubble_toplevel =
          InfoBubbleGtk::GetToplevelForInfoBubble(active_window);
      bool is_active = (GTK_WIDGET(window_)->window == active_window ||
                       (window_ == info_bubble_toplevel &&
                        use_custom_frame_.GetValue()));
      bool changed = (is_active != is_active_);
      is_active_ = is_active;
      if (changed) {
        SetBackgroundColor();
        gdk_window_invalidate_rect(GTK_WIDGET(window_)->window,
                                   &GTK_WIDGET(window_)->allocation, TRUE);
        // For some reason, the above two calls cause the window shape to be
        // lost so reset it.
        UpdateWindowShape(bounds_.width(), bounds_.height());
      }
      break;
    }

    default:
      NOTREACHED() << "Got a notification we didn't register for!";
  }
}

void BrowserWindowGtk::TabDetachedAt(TabContents* contents, int index) {
  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetSelectedTabContents() will return NULL or something else.
  if (index == browser_->tabstrip_model()->selected_index())
    infobar_container_->ChangeTabContents(NULL);
  contents_container_->DetachTabContents(contents);
  UpdateDevToolsForContents(NULL);
}

void BrowserWindowGtk::TabSelectedAt(TabContents* old_contents,
                                     TabContents* new_contents,
                                     int index,
                                     bool user_gesture) {
  DCHECK(old_contents != new_contents);

  if (old_contents && !old_contents->is_being_destroyed())
    old_contents->view()->StoreFocus();

  // Update various elements that are interested in knowing the current
  // TabContents.
  infobar_container_->ChangeTabContents(new_contents);
  contents_container_->SetTabContents(new_contents);
  UpdateDevToolsForContents(new_contents);

  new_contents->DidBecomeSelected();
  // TODO(estade): after we manage browser activation, add a check to make sure
  // we are the active browser before calling RestoreFocus().
  if (!browser_->tabstrip_model()->closing_all()) {
    new_contents->view()->RestoreFocus();
    if (new_contents->find_ui_active())
      browser_->find_bar()->find_bar()->SetFocusAndSelection();
  }

  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);
}

void BrowserWindowGtk::TabStripEmpty() {
  UpdateUIForContents(NULL);
}

void BrowserWindowGtk::MaybeShowBookmarkBar(TabContents* contents,
                                            bool animate) {
  // Don't change the visibility state when the browser is full screen or if
  // the bookmark bar isn't supported.
  if (IsFullscreen() || !IsBookmarkBarSupported())
    return;

  bool show_bar = false;

  if (browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)
      && contents) {
    bookmark_bar_->SetProfile(contents->profile());
    bookmark_bar_->SetPageNavigator(contents);
    show_bar = true;
  }

  if (show_bar && !contents->IsBookmarkBarAlwaysVisible()) {
    PrefService* prefs = contents->profile()->GetPrefs();
    show_bar = prefs->GetBoolean(prefs::kShowBookmarkBar);
  }

  if (show_bar) {
    bookmark_bar_->Show(animate);
  } else {
    bookmark_bar_->Hide(animate);
  }
}

void BrowserWindowGtk::MaybeShowExtensionShelf() {
  if (extension_shelf_.get())
    extension_shelf_->Show();
}

void BrowserWindowGtk::UpdateDevToolsForContents(TabContents* contents) {
  TabContents* old_devtools = devtools_container_->GetTabContents();
  if (old_devtools)
    devtools_container_->DetachTabContents(old_devtools);

  TabContents* devtools_contents = contents ?
      DevToolsWindow::GetDevToolsContents(contents) : NULL;
  devtools_container_->SetTabContents(devtools_contents);
  if (devtools_contents) {
    // TabContentsViewGtk::WasShown is not called when tab contents is shown by
    // anything other than user selecting a Tab.
    // See TabContentsViewWin::OnWindowPosChanged for reference on how it should
    // be implemented.
    devtools_contents->ShowContents();
  }

  bool should_show = old_devtools == NULL && devtools_contents != NULL;
  bool should_hide = old_devtools != NULL && devtools_contents == NULL;
  if (should_show) {
    gtk_widget_show(devtools_container_->widget());
  } else if (should_hide) {
    // Store split offset when hiding devtools window only.
    gint divider_offset = gtk_paned_get_position(GTK_PANED(contents_split_));
    g_browser_process->local_state()->SetInteger(
        prefs::kDevToolsSplitLocation, divider_offset);
    gtk_widget_hide(devtools_container_->widget());
  }
}

void BrowserWindowGtk::UpdateUIForContents(TabContents* contents) {
  MaybeShowBookmarkBar(contents, false);
}

void BrowserWindowGtk::DestroyBrowser() {
  browser_.reset();
}

void BrowserWindowGtk::OnBoundsChanged(const gfx::Rect& bounds) {
  if (bounds_.size() != bounds.size())
    OnSizeChanged(bounds.width(), bounds.height());

  bounds_ = bounds;
  if (!IsFullscreen() && !IsMaximized())
    restored_bounds_ = bounds;
  SaveWindowPosition();
}

void BrowserWindowGtk::OnStateChanged(GdkWindowState state) {
  // If we care about more than full screen changes, we should pass through
  // |changed_mask| from GdkEventWindowState.
  bool fullscreen_state_changed = (state_ & GDK_WINDOW_STATE_FULLSCREEN) !=
      (state & GDK_WINDOW_STATE_FULLSCREEN);

  state_ = state;

  if (fullscreen_state_changed) {
    bool is_fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
    browser_->UpdateCommandsForFullscreenMode(is_fullscreen);
    if (is_fullscreen) {
      UpdateCustomFrame();
      toolbar_->Hide();
      tabstrip_->Hide();
      bookmark_bar_->Hide(false);
      if (extension_shelf_.get())
        extension_shelf_->Hide();
    } else {
      UpdateCustomFrame();
      ShowSupportedWindowFeatures();
    }
  }

  UpdateWindowShape(bounds_.width(), bounds_.height());
  SaveWindowPosition();
}

bool BrowserWindowGtk::CanClose() const {
#if defined(OS_CHROMEOS)
  if (drag_active_)
    return false;
#endif

  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_->IsDragSessionActive())
    return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the window (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    gtk_widget_hide(GTK_WIDGET(window_));
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  NotificationService::current()->Notify(
      NotificationType::WINDOW_CLOSED,
      Source<GtkWindow>(window_),
      NotificationService::NoDetails());
  return true;
}

bool BrowserWindowGtk::ShouldShowWindowIcon() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

void BrowserWindowGtk::AddFindBar(FindBarGtk* findbar) {
  gtk_box_pack_start(GTK_BOX(render_area_vbox_), findbar->widget(),
                     FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(render_area_vbox_), findbar->widget(), 0);
}

void BrowserWindowGtk::ResetCustomFrameCursor() {
  if (!frame_cursor_)
    return;

  gdk_cursor_unref(frame_cursor_);
  frame_cursor_ = NULL;
  gdk_window_set_cursor(GTK_WIDGET(window_)->window, NULL);
}

// static
BrowserWindowGtk* BrowserWindowGtk::GetBrowserWindowForNativeWindow(
    gfx::NativeWindow window) {
  if (window) {
    return static_cast<BrowserWindowGtk*>(
        g_object_get_data(G_OBJECT(window), kBrowserWindowKey));
  }

  return NULL;
}

// static
GtkWindow* BrowserWindowGtk::GetBrowserWindowForXID(XID xid) {
  return BrowserWindowGtk::xid_map_.find(xid)->second;
}

// static
void BrowserWindowGtk::RegisterUserPrefs(PrefService* prefs) {
  bool custom_frame_default = false;
  // Avoid checking the window manager if we're not connected to an X server (as
  // is the case in Valgrind tests).
  if (x11_util::XDisplayExists() &&
      !prefs->HasPrefPath(prefs::kUseCustomChromeFrame)) {
    custom_frame_default = GetCustomFramePrefDefault();
  }
  prefs->RegisterBooleanPref(
      prefs::kUseCustomChromeFrame, custom_frame_default);
}

void BrowserWindowGtk::SetGeometryHints() {
  // Allow the user to resize us arbitrarily small.
  GdkGeometry geometry;
  geometry.min_width = 1;
  geometry.min_height = 1;
  gtk_window_set_geometry_hints(window_, NULL, &geometry, GDK_HINT_MIN_SIZE);

  // If we call gtk_window_maximize followed by gtk_window_present, compiz gets
  // confused and maximizes the window, but doesn't set the
  // GDK_WINDOW_STATE_MAXIMIZED bit.  So instead, we keep track of whether to
  // maximize and call it after gtk_window_present.
  maximize_after_show_ = browser_->GetSavedMaximizedState();

  gfx::Rect bounds = browser_->GetSavedWindowBounds();
  // We don't blindly call SetBounds here, that sets a forced position
  // on the window and we intentionally *don't* do that for normal
  // windows.  We tested many programs and none of them restored their
  // position on Linux.
  //
  // However, in cases like dropping a tab where the bounds are
  // specifically set, we do want to position explicitly.
  if (browser_->bounds_overridden()) {
    SetBounds(bounds);
  } else {
    // Ignore the position but obey the size.
    gtk_window_resize(window_, bounds.width(), bounds.height());
  }
}

void BrowserWindowGtk::ConnectHandlersToSignals() {
  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(MainWindowDeleteEvent), this);
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(MainWindowDestroy), this);
  g_signal_connect(window_, "configure-event",
                   G_CALLBACK(MainWindowConfigured), this);
  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(MainWindowStateChanged), this);
  g_signal_connect(window_, "map",
                   G_CALLBACK(MainWindowMapped), this);
  g_signal_connect(window_, "unmap",
                     G_CALLBACK(MainWindowUnMapped), this);
  g_signal_connect(window_, "key-press-event",
                   G_CALLBACK(OnKeyPress), browser_.get());
  g_signal_connect(window_, "motion-notify-event",
                   G_CALLBACK(OnMouseMoveEvent), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(OnButtonPressEvent), this);
  g_signal_connect(window_, "focus-in-event",
                   G_CALLBACK(OnFocusIn), this);
  g_signal_connect(window_, "focus-out-event",
                   G_CALLBACK(OnFocusOut), this);
}

void BrowserWindowGtk::InitWidgets() {
  // This vbox encompasses all of the widgets within the browser, including the
  // tabstrip and the content vbox.  The vbox is put in a floating container
  // (see gtk_floating_container.h) so we can position the
  // minimize/maximize/close buttons.  The floating container is then put in an
  // alignment so we can do custom frame drawing if the user turns of window
  // manager decorations.
  GtkWidget* window_vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(window_vbox);

  // The window container draws the custom browser frame.
  window_container_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_widget_set_app_paintable(window_container_, TRUE);
  gtk_widget_set_double_buffered(window_container_, FALSE);
  gtk_widget_set_redraw_on_allocate(window_container_, TRUE);
  g_signal_connect(G_OBJECT(window_container_), "expose-event",
                   G_CALLBACK(&OnCustomFrameExpose), this);
  gtk_container_add(GTK_CONTAINER(window_container_), window_vbox);

  tabstrip_.reset(new TabStripGtk(browser_->tabstrip_model()));
  tabstrip_->Init();

  // Build the titlebar (tabstrip + header space + min/max/close buttons).
  titlebar_.reset(new BrowserTitlebar(this, window_));

#if defined(OS_CHROMEOS) && defined(COMPACT_NAV_BAR)
  // Make a box that we'll later insert the compact navigation bar into. The
  // tabstrip must go into an hbox with our box so that they can get arranged
  // horizontally.
  GtkWidget* titlebar_hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget* navbar_hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget* status_hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(navbar_hbox);
  gtk_widget_show(titlebar_hbox);
  gtk_widget_show(status_hbox);
  gtk_box_pack_start(GTK_BOX(titlebar_hbox), navbar_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(titlebar_hbox), titlebar_->widget(), TRUE, TRUE,
                     0);
  gtk_box_pack_start(GTK_BOX(titlebar_hbox), status_hbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(window_vbox), titlebar_hbox, FALSE, FALSE, 0);

#else
  // Insert the tabstrip into the window.
  gtk_box_pack_start(GTK_BOX(window_vbox), titlebar_->widget(), FALSE, FALSE,
                     0);
#endif  // OS_CHROMEOS

  // The content_vbox_ surrounds the "content": toolbar+bookmarks bar+page.
  content_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(content_vbox_);

  toolbar_.reset(new BrowserToolbarGtk(browser_.get(), this));
  toolbar_->Init(browser_->profile(), window_);
  toolbar_->AddToolbarToBox(content_vbox_);
#if defined(OS_CHROMEOS) && defined(COMPACT_NAV_BAR)
  gtk_widget_hide(toolbar_->widget());
#endif

  bookmark_bar_.reset(new BookmarkBarGtk(browser_->profile(), browser_.get(),
                                         this));
  bookmark_bar_->AddBookmarkbarToBox(content_vbox_);

  if (IsExtensionShelfSupported()) {
    extension_shelf_.reset(new ExtensionShelfGtk(browser()->profile(),
                                                 browser_.get()));
    extension_shelf_->AddShelfToBox(content_vbox_);
    MaybeShowExtensionShelf();
  }

  // This vbox surrounds the render area: find bar, info bars and render view.
  // The reason is that this area as a whole needs to be grouped in its own
  // GdkWindow hierarchy so that animations originating inside it (infobar,
  // download shelf, find bar) are all clipped to that area. This is why
  // |render_area_vbox_| is packed in |event_box|.
  render_area_vbox_ = gtk_vbox_new(FALSE, 0);
  infobar_container_.reset(new InfoBarContainerGtk(this));
  gtk_box_pack_start(GTK_BOX(render_area_vbox_),
                     infobar_container_->widget(),
                     FALSE, FALSE, 0);

  status_bubble_.reset(new StatusBubbleGtk(browser_->profile()));

  contents_container_.reset(new TabContentsContainerGtk(status_bubble_.get()));
  devtools_container_.reset(new TabContentsContainerGtk(NULL));
  contents_split_ = gtk_vpaned_new();
  gtk_paned_pack1(GTK_PANED(contents_split_), contents_container_->widget(),
                  TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(contents_split_), devtools_container_->widget(),
                  FALSE, TRUE);
  gtk_box_pack_start(GTK_BOX(render_area_vbox_), contents_split_, TRUE, TRUE,
                     0);
  // Restore split offset.
  int split_offset = g_browser_process->local_state()->GetInteger(
      prefs::kDevToolsSplitLocation);
  if (split_offset == -1) {
    // Initial load, set to default value.
    split_offset = kDefaultContentsSplitOffset;
  }
  gtk_paned_set_position(GTK_PANED(contents_split_), split_offset);
  gtk_widget_show_all(render_area_vbox_);
  gtk_widget_hide(devtools_container_->widget());

#if defined(OS_CHROMEOS)
  if (browser_->type() == Browser::TYPE_POPUP) {
    toolbar_->Hide();
    // The window manager needs the min size for popups
    gtk_widget_set_size_request(
        GTK_WIDGET(window_), bounds_.width(), bounds_.height());
  }
#endif

  // We have to realize the window before we try to apply a window shape mask.
  gtk_widget_realize(GTK_WIDGET(window_));
  state_ = gdk_window_get_state(GTK_WIDGET(window_)->window);
  // Note that calling this the first time is necessary to get the
  // proper control layout.
  UpdateCustomFrame();

  GtkWidget* event_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(event_box), render_area_vbox_);
  gtk_widget_show(event_box);
  gtk_container_add(GTK_CONTAINER(content_vbox_), event_box);
  gtk_container_add(GTK_CONTAINER(window_vbox), content_vbox_);
  gtk_container_add(GTK_CONTAINER(window_), window_container_);
  gtk_widget_show(window_container_);
  browser_->tabstrip_model()->AddObserver(this);

#if defined(OS_CHROMEOS) && defined(COMPACT_NAV_BAR)
  // Create the compact navigation bar. This must be done after adding
  // everything to the window since it's done in Views, which expects to call
  // realize (requiring a window) in the Init function.
  views::WidgetGtk* clb_widget =
      new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
  clb_widget->set_delete_on_destroy(true);
  // Must initialize with a NULL parent since the widget will assume the parent
  // is also a WidgetGtk. Then we can parent the native widget afterwards.
  clb_widget->Init(NULL, gfx::Rect(0, 0, 100, 30));
  gtk_widget_reparent(clb_widget->GetNativeView(), navbar_hbox);

  compact_navigation_bar_ = new CompactNavigationBar(browser_.get());

  clb_widget->SetContentsView(compact_navigation_bar_);
  compact_navigation_bar_->Init();

  // Create the status area.
  views::WidgetGtk* status_widget =
      new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
  status_widget->set_delete_on_destroy(true);
  status_widget->Init(NULL, gfx::Rect(0, 0, 100, 30));
  gtk_widget_reparent(status_widget->GetNativeView(), status_hbox);
  status_area_ = new StatusAreaView(browser());
  status_widget->SetContentsView(status_area_);
  status_area_->Init();

  // Must be after Init.
  gtk_widget_set_size_request(clb_widget->GetNativeView(),
      compact_navigation_bar_->GetPreferredSize().width(), 20);
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  gtk_widget_set_size_request(status_widget->GetNativeView(),
                              status_area_size.width(),
                              status_area_size.height());
  clb_widget->Show();
  status_widget->Show();
#endif  // OS_CHROMEOS
}

void BrowserWindowGtk::SetBackgroundColor() {
  Profile* profile = browser()->profile();
  ThemeProvider* theme_provider = profile->GetThemeProvider();
  int frame_color_id;
  if (IsActive()) {
    frame_color_id = browser()->profile()->IsOffTheRecord()
        ? BrowserThemeProvider::COLOR_FRAME_INCOGNITO
        : BrowserThemeProvider::COLOR_FRAME;
  } else {
    frame_color_id = browser()->profile()->IsOffTheRecord()
        ? BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE
        : BrowserThemeProvider::COLOR_FRAME_INACTIVE;
  }

  SkColor frame_color = theme_provider->GetColor(frame_color_id);

  // Paint the frame color on the left, right and bottom.
  GdkColor frame_color_gdk = GDK_COLOR_RGB(SkColorGetR(frame_color),
      SkColorGetG(frame_color), SkColorGetB(frame_color));
  gtk_widget_modify_bg(GTK_WIDGET(window_), GTK_STATE_NORMAL,
                       &frame_color_gdk);
}

void BrowserWindowGtk::OnSizeChanged(int width, int height) {
  UpdateWindowShape(width, height);
}

void BrowserWindowGtk::UpdateWindowShape(int width, int height) {
  if (use_custom_frame_.GetValue() && !IsFullscreen() && !IsMaximized()) {
    // Make the top corners rounded.  We set a mask that includes most of the
    // window except for a few pixels in the top two corners.
    GdkRectangle top_rect = { 3, 0, width - 6, 1 };
    GdkRectangle mid_rect = { 1, 1, width - 2, 2 };
    GdkRectangle bot_rect = { 0, 3, width, height - 3 };
    GdkRegion* mask = gdk_region_rectangle(&top_rect);
    gdk_region_union_with_rect(mask, &mid_rect);
    gdk_region_union_with_rect(mask, &bot_rect);
    gdk_window_shape_combine_region(GTK_WIDGET(window_)->window, mask, 0, 0);
    gdk_region_destroy(mask);
    gtk_alignment_set_padding(GTK_ALIGNMENT(window_container_), 1,
        kFrameBorderThickness, kFrameBorderThickness, kFrameBorderThickness);
  } else {
    // XFCE disables the system decorations if there's an xshape set.
    if (use_custom_frame_.GetValue()) {
      // Disable rounded corners.  Simply passing in a NULL region doesn't
      // seem to work on KWin, so manually set the shape to the whole window.
      GdkRectangle rect = { 0, 0, width, height };
      GdkRegion* mask = gdk_region_rectangle(&rect);
      gdk_window_shape_combine_region(GTK_WIDGET(window_)->window, mask, 0, 0);
      gdk_region_destroy(mask);
    } else {
      gdk_window_shape_combine_region(GTK_WIDGET(window_)->window, NULL, 0, 0);
    }
    gtk_alignment_set_padding(GTK_ALIGNMENT(window_container_), 0, 0, 0, 0);
  }
}

void BrowserWindowGtk::ConnectAccelerators() {
  accel_group_ = gtk_accel_group_new();
  gtk_window_add_accel_group(window_, accel_group_);

  for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
    gtk_accel_group_connect(
        accel_group_,
        kAcceleratorMap[i].keyval,
        kAcceleratorMap[i].modifier_type,
        GtkAccelFlags(0),
        g_cclosure_new(G_CALLBACK(OnGtkAccelerator), this, NULL));
  }
}

void BrowserWindowGtk::UpdateCustomFrame() {
  bool enable = use_custom_frame_.GetValue() && !IsFullscreen();
  gtk_window_set_decorated(window_, !use_custom_frame_.GetValue());
  titlebar_->UpdateCustomFrame(enable);
  UpdateWindowShape(bounds_.width(), bounds_.height());
}

void BrowserWindowGtk::SaveWindowPosition() {
  // Browser::SaveWindowPlacement is used for session restore.
  if (browser_->ShouldSaveWindowPlacement())
    browser_->SaveWindowPlacement(restored_bounds_, IsMaximized());

  // We also need to save the placement for startup.
  // This is a web of calls between views and delegates on Windows, but the
  // crux of the logic follows.  See also cocoa/browser_window_controller.mm.
  if (!g_browser_process->local_state())
    return;

  std::wstring window_name = browser_->GetWindowPlacementKey();
  DictionaryValue* window_preferences =
      g_browser_process->local_state()->GetMutableDictionary(
          window_name.c_str());
  // Note that we store left/top for consistency with Windows, but that we
  // *don't* obey them; we only use them for computing width/height.  See
  // comments in SetGeometryHints().
  window_preferences->SetInteger(L"left", restored_bounds_.x());
  window_preferences->SetInteger(L"top", restored_bounds_.y());
  window_preferences->SetInteger(L"right", restored_bounds_.right());
  window_preferences->SetInteger(L"bottom", restored_bounds_.bottom());
  window_preferences->SetBoolean(L"maximized", IsMaximized());

  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect work_area(
      monitor_info_provider->GetMonitorWorkAreaMatching(restored_bounds_));
  window_preferences->SetInteger(L"work_area_left", work_area.x());
  window_preferences->SetInteger(L"work_area_top", work_area.y());
  window_preferences->SetInteger(L"work_area_right", work_area.right());
  window_preferences->SetInteger(L"work_area_bottom", work_area.bottom());
}

// static
gboolean BrowserWindowGtk::OnGtkAccelerator(GtkAccelGroup* accel_group,
                                            GObject* acceleratable,
                                            guint keyval,
                                            GdkModifierType modifier,
                                            BrowserWindowGtk* browser_window) {
  int command_id = GetCommandId(keyval, modifier);
  browser_window->ExecuteBrowserCommand(command_id);

  return TRUE;
}

// static
gboolean BrowserWindowGtk::OnMouseMoveEvent(GtkWidget* widget,
    GdkEventMotion* event, BrowserWindowGtk* browser) {
  // This method is used to update the mouse cursor when over the edge of the
  // custom frame.  If the custom frame is off or we're over some other widget,
  // do nothing.
  if (!browser->use_custom_frame_.GetValue() ||
      event->window != widget->window) {
    // Reset the cursor.
    if (browser->frame_cursor_) {
      gdk_cursor_unref(browser->frame_cursor_);
      browser->frame_cursor_ = NULL;
      gdk_window_set_cursor(GTK_WIDGET(browser->window_)->window, NULL);
    }
    return FALSE;
  }

  // Update the cursor if we're on the custom frame border.
  GdkWindowEdge edge;
  bool has_hit_edge = browser->GetWindowEdge(static_cast<int>(event->x),
      static_cast<int>(event->y), &edge);
  GdkCursorType new_cursor = GDK_LAST_CURSOR;
  if (has_hit_edge)
    new_cursor = GdkWindowEdgeToGdkCursorType(edge);

  GdkCursorType last_cursor = GDK_LAST_CURSOR;
  if (browser->frame_cursor_)
    last_cursor = browser->frame_cursor_->type;

  if (last_cursor != new_cursor) {
    if (browser->frame_cursor_) {
      gdk_cursor_unref(browser->frame_cursor_);
      browser->frame_cursor_ = NULL;
    }
    if (has_hit_edge) {
      browser->frame_cursor_ = gdk_cursor_new(new_cursor);
      gdk_window_set_cursor(GTK_WIDGET(browser->window_)->window,
                            browser->frame_cursor_);
    } else {
      gdk_window_set_cursor(GTK_WIDGET(browser->window_)->window, NULL);
    }
  }
  return FALSE;
}

// static
gboolean BrowserWindowGtk::OnButtonPressEvent(GtkWidget* widget,
    GdkEventButton* event, BrowserWindowGtk* browser) {
  // Handle back/forward.
  // TODO(jhawkins): Investigate the possibility of the button numbers being
  // different for other mice.
  if (event->button == 8) {
    browser->browser_->GoBack(CURRENT_TAB);
    return TRUE;
  } else if (event->button == 9) {
    browser->browser_->GoForward(CURRENT_TAB);
    return TRUE;
  }

  // Handle left, middle and right clicks.  In particular, we care about clicks
  // in the custom frame border and clicks in the titlebar.
  GdkWindowEdge edge;
  bool has_hit_edge = browser->GetWindowEdge(static_cast<int>(event->x),
      static_cast<int>(event->y), &edge);
  // Ignore clicks that are in/below the browser toolbar.
  GtkWidget* toolbar = browser->toolbar_->widget();
  if (!GTK_WIDGET_VISIBLE(toolbar)) {
    // If the toolbar is not showing, use the location of web contents as the
    // boundary of where to ignore clicks.
    toolbar = browser->render_area_vbox_;
  }
  gint toolbar_y;
  gtk_widget_get_pointer(toolbar, NULL, &toolbar_y);
  bool has_hit_titlebar = !browser->IsFullscreen() && (toolbar_y < 0)
      && !has_hit_edge;
  if (event->button == 1) {
    if (GDK_BUTTON_PRESS == event->type) {
      guint32 last_click_time = browser->last_click_time_;
      gfx::Point last_click_position = browser->last_click_position_;
      browser->last_click_time_ = event->time;
      browser->last_click_position_ = gfx::Point(event->x, event->y);

      if (has_hit_titlebar) {
        // We want to start a move when the user single clicks, but not start a
        // move when the user double clicks.  However, a double click sends the
        // following GDK events: GDK_BUTTON_PRESS, GDK_BUTTON_RELEASE,
        // GDK_BUTTON_PRESS, GDK_2BUTTON_PRESS, GDK_BUTTON_RELEASE.  If we
        // start a gtk_window_begin_move_drag on the second GDK_BUTTON_PRESS,
        // the call to gtk_window_maximize fails.  To work around this, we
        // keep track of the last click and if it's going to be a double click,
        // we don't call gtk_window_begin_move_drag.
        static GtkSettings* settings = gtk_settings_get_default();
        gint double_click_time = 250;
        gint double_click_distance = 5;
        g_object_get(G_OBJECT(settings),
            "gtk-double-click-time", &double_click_time,
            "gtk-double-click-distance", &double_click_distance,
            NULL);

        guint32 click_time = event->time - last_click_time;
        int click_move_x = event->x - last_click_position.x();
        int click_move_y = event->y - last_click_position.y();

        if (click_time > static_cast<guint32>(double_click_time) ||
            click_move_x > double_click_distance ||
            click_move_y > double_click_distance) {
          gtk_window_begin_move_drag(browser->window_, event->button,
                                     event->x_root, event->y_root, event->time);
          return TRUE;
        }
      } else if (has_hit_edge) {
        gtk_window_begin_resize_drag(browser->window_, edge, event->button,
                                     event->x_root, event->y_root, event->time);
        return TRUE;
      }
    } else if (GDK_2BUTTON_PRESS == event->type) {
      if (has_hit_titlebar) {
        // Maximize/restore on double click.
        if (browser->IsMaximized()) {
          gtk_window_unmaximize(browser->window_);
        } else {
          gtk_window_maximize(browser->window_);
        }
        return TRUE;
      }
    }
  } else if (event->button == 2) {
    if (has_hit_titlebar || has_hit_edge) {
      gdk_window_lower(GTK_WIDGET(browser->window_)->window);
    }
    return TRUE;
  } else if (event->button == 3) {
    if (has_hit_titlebar) {
      browser->titlebar_->ShowContextMenu();
      return TRUE;
    }
  }

  return FALSE;  // Continue to propagate the event.
}

// static
void BrowserWindowGtk::MainWindowMapped(GtkWidget* widget,
                                        BrowserWindowGtk* window) {
  // Map the X Window ID of the window to our window.
  XID xid = x11_util::GetX11WindowFromGtkWidget(widget);
  BrowserWindowGtk::xid_map_.insert(
      std::pair<XID, GtkWindow*>(xid, GTK_WINDOW(widget)));
}

// static
void BrowserWindowGtk::MainWindowUnMapped(GtkWidget* widget,
                                          BrowserWindowGtk* window) {
  // Unmap the X Window ID.
  XID xid = x11_util::GetX11WindowFromGtkWidget(widget);
  BrowserWindowGtk::xid_map_.erase(xid);
}

// static
gboolean BrowserWindowGtk::OnFocusIn(GtkWidget* widget,
                                     GdkEventFocus* event,
                                     BrowserWindowGtk* browser) {
  BrowserList::SetLastActive(browser->browser_.get());
#if defined(OS_CHROMEOS)
  if (browser->panel_controller_) {
    browser->panel_controller_->OnFocusIn();
  }
#endif
  return FALSE;
}

// static
gboolean BrowserWindowGtk::OnFocusOut(GtkWidget* widget,
                                      GdkEventFocus* event,
                                      BrowserWindowGtk* browser) {
#if defined(OS_CHROMEOS)
  if (browser->panel_controller_) {
    browser->panel_controller_->OnFocusOut();
  }
#endif
  return FALSE;
}

void BrowserWindowGtk::ExecuteBrowserCommand(int id) {
  if (browser_->command_updater()->IsCommandEnabled(id))
    browser_->ExecuteCommand(id);
}

void BrowserWindowGtk::ShowSupportedWindowFeatures() {
  if (IsTabStripSupported())
    tabstrip_->Show();

  if (IsToolbarSupported())
    toolbar_->Show();

  if (IsBookmarkBarSupported())
    MaybeShowBookmarkBar(browser_->GetSelectedTabContents(), false);

  if (IsExtensionShelfSupported())
    MaybeShowExtensionShelf();
}

void BrowserWindowGtk::HideUnsupportedWindowFeatures() {
  if (!IsTabStripSupported())
    tabstrip_->Hide();

  if (!IsToolbarSupported())
    toolbar_->Hide();

  if (!IsBookmarkBarSupported())
    bookmark_bar_->Hide(false);

  if (!IsExtensionShelfSupported() && extension_shelf_.get())
    extension_shelf_->Hide();
}

bool BrowserWindowGtk::IsTabStripSupported() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserWindowGtk::IsToolbarSupported() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

bool BrowserWindowGtk::IsBookmarkBarSupported() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR);
}

bool BrowserWindowGtk::IsExtensionShelfSupported() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_EXTENSIONSHELF);
}

bool BrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  if (!use_custom_frame_.GetValue())
    return false;

  if (IsMaximized() || IsFullscreen())
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
    return true;
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
    return true;
  } else {
    // Right edge.
    if (y < kResizeAreaCornerSize - kTopResizeAdjust) {
      *edge = GDK_WINDOW_EDGE_NORTH_EAST;
    } else if (y < bounds_.height() - kResizeAreaCornerSize) {
      *edge = GDK_WINDOW_EDGE_EAST;
    } else {
      *edge = GDK_WINDOW_EDGE_SOUTH_EAST;
    }
    return true;
  }
  NOTREACHED();
}

// static
bool BrowserWindowGtk::GetCustomFramePrefDefault() {
  int wm_window = 0;
  if (!x11_util::GetIntProperty(x11_util::GetX11RootWindow(),
                                "_NET_SUPPORTING_WM_CHECK",
                                &wm_window)) {
    return false;
  }

  std::string wm_name;
  if (!x11_util::GetStringProperty(static_cast<XID>(wm_window),
                                   "_NET_WM_NAME",
                                   &wm_name)) {
    return false;
  }

  // Ideally, we'd use the custom frame by default and just fall back on using
  // system decorations for the few (?) tiling window managers where it doesn't
  // make sense (e.g. awesome, ion3, ratpoison, xmonad, etc.).  The EWMH
  // _NET_SUPPORTING_WM property makes it easy to look up a name for the current
  // WM, but at least some of the WMs in the latter group don't set it.
  // Instead, we default to using system decorations for all WMs and
  // special-case the ones where the custom frame should be used.  These names
  // are taken from the WMs' source code.
  return (wm_name == "Blackbox" ||
          wm_name == "compiz" ||
          wm_name == "e16" ||  // Enlightenment DR16
          wm_name == "Fluxbox" ||
          wm_name == "KWin" ||
          wm_name == "Metacity" ||
          wm_name == "Openbox" ||
          wm_name == "Xfwm4");
}
