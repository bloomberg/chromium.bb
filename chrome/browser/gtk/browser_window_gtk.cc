// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_window_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include <string>

#include "app/gfx/color_utils.h"
#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/base_paths_linux.h"
#include "base/command_line.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
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
#include "chrome/browser/gtk/bookmark_bar_gtk.h"
#include "chrome/browser/gtk/bookmark_manager_gtk.h"
#include "chrome/browser/gtk/browser_titlebar.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/clear_browsing_data_dialog_gtk.h"
#include "chrome/browser/gtk/create_application_shortcuts_dialog_gtk.h"
#include "chrome/browser/gtk/download_in_progress_dialog_gtk.h"
#include "chrome/browser/gtk/download_shelf_gtk.h"
#include "chrome/browser/gtk/edit_search_engine_dialog.h"
#include "chrome/browser/gtk/find_bar_gtk.h"
#include "chrome/browser/gtk/fullscreen_exit_bubble_gtk.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/go_button_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/import_dialog_gtk.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"
#include "chrome/browser/gtk/keyword_editor_view.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/repost_form_warning_gtk.h"
#include "chrome/browser/gtk/status_bubble_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/gtk/task_manager_gtk.h"
#include "chrome/browser/gtk/theme_install_bubble_view_gtk.h"
#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_gtk.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/compact_navigation_bar.h"
#include "chrome/browser/chromeos/main_menu.h"
#include "chrome/browser/chromeos/panel_controller.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "views/widget/widget_gtk.h"
#endif

namespace {

// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;

// Default height of dev tools pane when docked to the browser window.  This
// matches the value in Views.
const int kDefaultDevToolsHeight = 200;

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
// The offset to the background when the custom frame is off.  We want the
// window background to line up with the tab background regardless of whether
// we're in custom frame mode or not.  Since themes are designed with the
// custom frame in mind, we need to offset the background when the custom frame
// is off.
const int kCustomFrameBackgroundVerticalOffset = 15;

// The timeout in milliseconds before we'll get the true window position with
// gtk_window_get_position() after the last GTK configure-event signal.
const int kDebounceTimeoutMilliseconds = 100;

gboolean MainWindowConfigured(GtkWindow* window, GdkEventConfigure* event,
                              BrowserWindowGtk* browser_win) {
  gfx::Rect bounds = gfx::Rect(event->x, event->y, event->width, event->height);
  browser_win->OnBoundsChanged(bounds);
  return FALSE;
}

gboolean MainWindowStateChanged(GtkWindow* window, GdkEventWindowState* event,
                                BrowserWindowGtk* browser_win) {
  browser_win->OnStateChanged(event->new_window_state, event->changed_mask);
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
  { GDK_Page_Down, IDC_MOVE_TAB_NEXT,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_Page_Up, IDC_MOVE_TAB_PREVIOUS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_Page_Up, IDC_SELECT_PREVIOUS_TAB, GDK_CONTROL_MASK },
  { GDK_t, IDC_NEW_TAB, GDK_CONTROL_MASK },
  { GDK_n, IDC_NEW_WINDOW, GDK_CONTROL_MASK },
  { GDK_n, IDC_NEW_INCOGNITO_WINDOW,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
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
  { GDK_d, IDC_BOOKMARK_ALL_TABS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_d, IDC_BOOKMARK_PAGE, GDK_CONTROL_MASK },
  { XF86XK_AddFavorite, IDC_BOOKMARK_PAGE, GdkModifierType(0) },
  { XF86XK_Favorites, IDC_SHOW_BOOKMARK_BAR, GdkModifierType(0) },
  { XF86XK_History, IDC_SHOW_HISTORY, GdkModifierType(0) },
  { GDK_o, IDC_OPEN_FILE, GDK_CONTROL_MASK },
  { GDK_F11, IDC_FULLSCREEN, GdkModifierType(0) },
  { GDK_u, IDC_VIEW_SOURCE, GDK_CONTROL_MASK },
  { GDK_p, IDC_PRINT, GDK_CONTROL_MASK },
  { GDK_Escape, IDC_TASK_MANAGER, GDK_SHIFT_MASK },
  { GDK_Delete, IDC_CLEAR_BROWSING_DATA,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },

#if defined(OS_CHROMEOS)
  { GDK_f, IDC_FULLSCREEN,
    GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK) },
  { GDK_Delete, IDC_TASK_MANAGER,
    GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK) },
  { GDK_comma, IDC_CONTROL_PANEL, GdkModifierType(GDK_CONTROL_MASK) },
#endif
};

#if defined(OS_CHROMEOS)

namespace {

// This draws the spacer below the tab strip when we're using the compact
// location bar (i.e. no location bar). This basically duplicates the painting
// that the tab strip would have done for this region so that it blends
// nicely in with the bottom of the tabs.
gboolean OnCompactNavSpacerExpose(GtkWidget* widget,
                                  GdkEventExpose* e,
                                  BrowserWindowGtk* window) {
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, e->area.x, e->area.y, e->area.width, e->area.height);
  cairo_clip(cr);
  // The toolbar is supposed to blend in with the active tab, so we have to pass
  // coordinates for the IDR_THEME_TOOLBAR bitmap relative to the top of the
  // tab strip.
  gfx::Point tabstrip_origin =
      window->tabstrip()->GetTabStripOriginForWidget(widget);
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(
      window->browser()->profile());
  CairoCachedSurface* background = theme_provider->GetSurfaceNamed(
      IDR_THEME_TOOLBAR, widget);
  background->SetSource(cr, tabstrip_origin.x(), tabstrip_origin.y());
  // We tile the toolbar background in both directions.
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr,
      tabstrip_origin.x(),
      tabstrip_origin.y(),
      e->area.x + e->area.width - tabstrip_origin.x(),
      e->area.y + e->area.height - tabstrip_origin.y());
  cairo_fill(cr);
  cairo_destroy(cr);

  return FALSE;
}

}  // namespace

// Callback from GTK when the user clicks the main menu button.
static void OnMainMenuButtonClicked(GtkWidget* widget,
                                    BrowserWindowGtk* browser) {
  chromeos::MainMenu::Show(browser->browser());
}

#endif  // OS_CHROMEOS

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

  return -1;
}

int GetCustomCommandId(guint keyval, GdkModifierType modifier) {
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
        return IDC_SELECT_NEXT_TAB;
      } else if ((GDK_CONTROL_MASK | GDK_SHIFT_MASK) == modifier) {
        return IDC_SELECT_PREVIOUS_TAB;
      }
      break;

    default:
      break;
  }
  return -1;
}

// An event handler for key press events.  We need to special case key
// combinations that are not valid gtk accelerators.  This function returns
// TRUE if it can handle the key press.
gboolean HandleCustomAccelerator(guint keyval, GdkModifierType modifier,
                                 Browser* browser) {
  int command = GetCustomCommandId(keyval, modifier);
  if (command == -1)
    return FALSE;

  browser->ExecuteCommand(command);
  return TRUE;
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
      } else if ((GDK_CONTROL_MASK | GDK_SHIFT_MASK) == modifier) {
        browser->ExecuteCommand(IDC_MOVE_TAB_NEXT);
        return TRUE;
      }
      break;

    case GDK_Page_Up:
      if (GDK_CONTROL_MASK == modifier) {
        browser->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
        return TRUE;
      } else if ((GDK_CONTROL_MASK | GDK_SHIFT_MASK) == modifier) {
        browser->ExecuteCommand(IDC_MOVE_TAB_PREVIOUS);
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

    // Propagate the key event to child widget first, so we don't override their
    // accelerators.
    if (!gtk_window_propagate_key_event(window, event)) {
      if (!gtk_window_activate_key(window, event)) {
        gtk_bindings_activate_event(GTK_OBJECT(window), event);
      }
    }
  } else {
    bool rv = gtk_window_propagate_key_event(window, event);
    DCHECK(rv);
  }

  // Prevents the default handler from handling this event.
  return TRUE;
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

GdkColor SkColorToGdkColor(const SkColor& color) {
  return skia::SkColorToGdkColor(color);
}

// A helper method for setting the GtkWindow size that should be used in place
// of calling gtk_window_resize directly.  This is done to avoid a WM "feature"
// where setting the window size to the screen size causes the WM to set the
// EWMH for full screen mode.
void SetWindowSize(GtkWindow* window, int width, int height) {
  GdkScreen* screen = gdk_screen_get_default();
  if (width == gdk_screen_get_width(screen) &&
      height == gdk_screen_get_height(screen)) {
    // Adjust the height so we don't trigger the WM feature.
    gtk_window_resize(window, width, height - 1);
  } else {
    gtk_window_resize(window, width, height);
  }
}

}  // namespace

std::map<XID, GtkWindow*> BrowserWindowGtk::xid_map_;

#if defined(OS_CHROMEOS)
// Default to using the regular window style.
bool BrowserWindowGtk::next_window_should_use_compact_nav_ = false;
#endif

BrowserWindowGtk::BrowserWindowGtk(Browser* browser)
    :  browser_(browser),
       state_(GDK_WINDOW_STATE_WITHDRAWN),
#if defined(OS_CHROMEOS)
       drag_active_(false),
       panel_controller_(NULL),
       compact_navigation_bar_(NULL),
       status_area_(NULL),
       main_menu_button_(NULL),
       compact_navbar_hbox_(NULL),
#endif
       frame_cursor_(NULL),
       is_active_(true),
       last_click_time_(0),
       maximize_after_show_(false),
       accel_group_(NULL) {
  use_custom_frame_pref_.Init(prefs::kUseCustomChromeFrame,
      browser_->profile()->GetPrefs(), this);

  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  g_object_set_data(G_OBJECT(window_), kBrowserWindowKey, this);
  gtk_widget_add_events(GTK_WIDGET(window_), GDK_BUTTON_PRESS_MASK |
                                             GDK_POINTER_MOTION_MASK);

  // Add this window to its own unique window group to allow for
  // window-to-parent modality.
  gtk_window_group_add_window(gtk_window_group_new(), window_);
  g_object_unref(gtk_window_get_group(window_));

  // For popups, we initialize widgets then set the window geometry, because
  // popups need the widgets inited before they can set the window size
  // properly. For other windows, we set the geometry first to prevent resize
  // flicker.
  if (browser_->type() & Browser::TYPE_POPUP) {
    InitWidgets();
    SetGeometryHints();
  } else {
    SetGeometryHints();
    InitWidgets();
  }

  ConnectAccelerators();

  // Set the initial background color of widgets.
  SetBackgroundColor();
  HideUnsupportedWindowFeatures();

  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());

  ActiveWindowWatcherX::AddObserver(this);
}

BrowserWindowGtk::~BrowserWindowGtk() {
  ActiveWindowWatcherX::RemoveObserver(this);

  browser_->tabstrip_model()->RemoveObserver(this);

  if (frame_cursor_) {
    gdk_cursor_unref(frame_cursor_);
    frame_cursor_ = NULL;
  }
}

bool BrowserWindowGtk::HandleKeyboardEvent(GdkEventKey* event) {
  // Handles a key event in following sequence:
  // 1. Our special key accelerators, such as ctrl-tab, etc.
  // 2. Gtk mnemonics and accelerators.
  // This sequence matches the default key press handler of GtkWindow.
  //
  // It's not necessary to care about the keyboard layout, as
  // gtk_window_activate_key() takes care of it automatically.
  if (!HandleCustomAccelerator(event->keyval, GdkModifierType(event->state),
                               browser_.get())) {
    return gtk_window_activate_key(window_, event);
  }
  return true;
}

gboolean BrowserWindowGtk::OnCustomFrameExpose(GtkWidget* widget,
                                               GdkEventExpose* event,
                                               BrowserWindowGtk* window) {
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(
      window->browser()->profile());

  // Draw the default background.
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y, event->area.width,
                  event->area.height);
  cairo_clip(cr);

  int image_name;
  if (window->IsActive()) {
    image_name = window->browser()->profile()->IsOffTheRecord() ?
                 IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
  } else {
    image_name = window->browser()->profile()->IsOffTheRecord() ?
                 IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE;
  }
  CairoCachedSurface* surface = theme_provider->GetSurfaceNamed(
      image_name, widget);
  if (event->area.y < surface->Height()) {
    surface->SetSource(cr,
        0,
        window->UseCustomFrame() ? 0 : -kCustomFrameBackgroundVerticalOffset);
    // The frame background isn't tiled vertically.
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr, event->area.x, event->area.y,
                        event->area.width, surface->Height() - event->area.y);
    cairo_fill(cr);
  }

  if (theme_provider->HasCustomImage(IDR_THEME_FRAME_OVERLAY)) {
    CairoCachedSurface* theme_overlay = theme_provider->GetSurfaceNamed(
        window->IsActive() ? IDR_THEME_FRAME_OVERLAY
                           : IDR_THEME_FRAME_OVERLAY_INACTIVE, widget);
    theme_overlay->SetSource(cr, 0, 0);
    cairo_paint(cr);
  }

  DrawContentShadow(cr, window);

  cairo_destroy(cr);

  if (window->UseCustomFrame() && !window->IsMaximized()) {
    static NineBox custom_frame_border(
        IDR_WINDOW_TOP_LEFT_CORNER,
        IDR_WINDOW_TOP_CENTER,
        IDR_WINDOW_TOP_RIGHT_CORNER,
        IDR_WINDOW_LEFT_SIDE,
        0,
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
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(
      window->browser()->profile());
  int left_x, top_y;
  gtk_widget_translate_coordinates(window->toolbar_->widget(),
      GTK_WIDGET(window->window_), 0, 0, &left_x,
      &top_y);
  int width = window->window_vbox_->allocation.width;

  CairoCachedSurface* top_center = theme_provider->GetSurfaceNamed(
      IDR_CONTENT_TOP_CENTER, GTK_WIDGET(window->window_));
  top_center->SetSource(cr, left_x, top_y - kContentShadowThickness);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr, left_x, top_y - kContentShadowThickness, width,
                  top_center->Height());
  cairo_fill(cr);

  // Only draw the rest of the shadow if the user has the custom frame enabled.
  if (!window->UseCustomFrame())
    return;

  // The top left corner has a width of 3 pixels. On Windows, the last column
  // of pixels overlap the toolbar. We just crop it off on Linux.  The top
  // corners extend to the base of the toolbar (one pixel above the dividing
  // line).
  int right_x = left_x + width;
  CairoCachedSurface* top_left = theme_provider->GetSurfaceNamed(
      IDR_CONTENT_TOP_LEFT_CORNER, GTK_WIDGET(window->window_));
  top_left->SetSource(
      cr, left_x - kContentShadowThickness, top_y - kContentShadowThickness);
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
  CairoCachedSurface* top_right = theme_provider->GetSurfaceNamed(
      IDR_CONTENT_TOP_RIGHT_CORNER, GTK_WIDGET(window->window_));
  top_right->SetSource(cr, right_x - 1, top_y - kContentShadowThickness);
  cairo_rectangle(cr,
      right_x,
      top_y - kContentShadowThickness,
      kContentShadowThickness,
      top_corner_height);
  cairo_fill(cr);

  // Fill in the sides.  As above, we only draw 2 of the 3 columns on Linux.
  int bottom_y;
  gtk_widget_translate_coordinates(window->window_vbox_,
      GTK_WIDGET(window->window_),
      0, window->window_vbox_->allocation.height,
      NULL, &bottom_y);
  // |side_y| is where to start drawing the side shadows.  The top corners draw
  // the sides down to the bottom of the toolbar.
  int side_y = top_y - kContentShadowThickness + top_corner_height;
  // |side_height| is how many pixels to draw for the side borders.  We do one
  // pixel before the bottom of the web contents because that extra pixel is
  // drawn by the bottom corners.
  int side_height = bottom_y - side_y - 1;
  if (side_height > 0) {
    CairoCachedSurface* left = theme_provider->GetSurfaceNamed(
        IDR_CONTENT_LEFT_SIDE, GTK_WIDGET(window->window_));
    left->SetSource(cr, left_x - kContentShadowThickness, side_y);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    cairo_rectangle(cr,
        left_x - kContentShadowThickness,
        side_y,
        kContentShadowThickness,
        side_height);
    cairo_fill(cr);

    CairoCachedSurface* right = theme_provider->GetSurfaceNamed(
        IDR_CONTENT_RIGHT_SIDE, GTK_WIDGET(window->window_));
    right->SetSource(cr, right_x - 1, side_y);
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
  CairoCachedSurface* bottom_left = theme_provider->GetSurfaceNamed(
      IDR_CONTENT_BOTTOM_LEFT_CORNER, GTK_WIDGET(window->window_));
  bottom_left->SetSource(cr, left_x - kContentShadowThickness, bottom_y - 1);
  cairo_paint(cr);

  CairoCachedSurface* bottom_right = theme_provider->GetSurfaceNamed(
      IDR_CONTENT_BOTTOM_RIGHT_CORNER, GTK_WIDGET(window->window_));
  bottom_right->SetSource(cr, right_x - 1, bottom_y - 1);
  cairo_paint(cr);

  // Finally, draw the bottom row. Since we don't overlap the contents, we clip
  // the top row of pixels.
  CairoCachedSurface* bottom = theme_provider->GetSurfaceNamed(
      IDR_CONTENT_BOTTOM_CENTER, GTK_WIDGET(window->window_));
  bottom->SetSource(cr, left_x + 1, bottom_y - 1);
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
    panel_controller_ = new chromeos::PanelController(this);
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

  // If we have sized the window by setting a size request for the render
  // area, then undo it so that the render view can later adjust its own
  // size.
  gtk_widget_set_size_request(contents_container_->widget(), -1, -1);
}

void BrowserWindowGtk::SetBoundsImpl(const gfx::Rect& bounds, bool exterior) {
  gint x = static_cast<gint>(bounds.x());
  gint y = static_cast<gint>(bounds.y());
  gint width = static_cast<gint>(bounds.width());
  gint height = static_cast<gint>(bounds.height());

  gtk_window_move(window_, x, y);

#if defined(OS_CHROMEOS)
  // In Chrome OS we need to get the popup size set here for the panel
  // to be displayed with its initial size correctly.
  SetWindowSize(window_, width, height);
#else
  if (exterior) {
    SetWindowSize(window_, width, height);
  } else {
    gtk_widget_set_size_request(contents_container_->widget(),
                                width, height);
  }
#endif
}

void BrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  SetBoundsImpl(bounds, true);
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

  // Cancel any pending callback from the window configure debounce timer.
  window_configure_debounce_timer_.Stop();

  GtkWidget* window = GTK_WIDGET(window_);
  // To help catch bugs in any event handlers that might get fired during the
  // destruction, set window_ to NULL before any handlers will run.
  window_ = NULL;
  titlebar_->set_window(NULL);
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

void BrowserWindowGtk::SelectedTabExtensionShelfSizeChanged() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::UpdateTitleBar() {
#if defined(OS_CHROMEOS)
  if (panel_controller_)
    panel_controller_->UpdateTitleBar();
#endif

  string16 title = browser_->GetWindowTitleForCurrentTab();
  gtk_window_set_title(window_, UTF16ToUTF8(title).c_str());
  if (ShouldShowWindowIcon())
    titlebar_->UpdateTitleAndIcon();

  // We need to update the bookmark bar state if we're navigating away from the
  // NTP and "always show bookmark bar" is not set.  On Windows,
  // UpdateTitleBar() causes a layout in BrowserView which checks to see if
  // the bookmarks bar should be shown.
  ShelfVisibilityChanged();
}

void BrowserWindowGtk::ShelfVisibilityChanged() {
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

bool BrowserWindowGtk::IsFullscreenBubbleVisible() const {
  return fullscreen_exit_bubble_.get() ? true : false;
}

LocationBar* BrowserWindowGtk::GetLocationBar() const {
  return toolbar_->GetLocationBar();
}

void BrowserWindowGtk::SetFocusToLocationBar() {
  if (!IsFullscreen()) {
#if defined(OS_CHROMEOS)
    if (compact_navigation_bar_) {
      compact_navigation_bar_->FocusLocation();
    } else {
      GetLocationBar()->FocusLocation();
    }
#else
    GetLocationBar()->FocusLocation();
#endif
  }
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

bool BrowserWindowGtk::IsToolbarVisible() const {
  return IsToolbarSupported();
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

void BrowserWindowGtk::ToggleExtensionShelf() {
  NOTIMPLEMENTED();
}

void BrowserWindowGtk::ShowAboutChromeDialog() {
#if defined(OS_CHROMEOS)
  browser::ShowAboutChromeView(window_, browser_->profile());
#else
  ShowAboutDialogForProfile(window_, browser_->profile());
#endif
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

void BrowserWindowGtk::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
  new RepostFormWarningGtk(GetNativeHandle(), &tab_contents->controller());
}

void BrowserWindowGtk::ShowProfileErrorDialog(int message_id) {
  std::string title = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string message = l10n_util::GetStringUTF8(message_id);
  GtkWidget* dialog = gtk_message_dialog_new(window_,
      static_cast<GtkDialogFlags>(0), GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
      "%s", message.c_str());
  gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show_all(dialog);
}

void BrowserWindowGtk::ShowThemeInstallBubble() {
  ThemeInstallBubbleViewGtk::Show(window_);
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
  int sum = infobar_container_->TotalHeightOfAnimatingBars();
  if (IsBookmarkBarSupported() && bookmark_bar_->IsAnimating())
    sum += bookmark_bar_->GetHeight();
  if (download_shelf_.get() && download_shelf_->IsClosing())
    sum += download_shelf_->GetHeight();
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

void BrowserWindowGtk::ShowPageMenu() {
  // On Windows, this is used to show the page menu for a keyboard accelerator
  // (Alt+e).  We connect the accelerator directly to the widget in
  // BrowserToolbarGtk.
}

void BrowserWindowGtk::ShowAppMenu() {
  // On Windows, this is used to show the page menu for a keyboard accelerator
  // (Alt+f).  We connect the accelerator directly to the widget in
  // BrowserToolbarGtk.
}

int BrowserWindowGtk::GetCommandId(const NativeWebKeyboardEvent& event) {
  if (!event.os_event)
    return -1;

  guint keyval = event.os_event->keyval;
  GdkModifierType modifier = GdkModifierType(event.os_event->state);
  int command = ::GetCommandId(keyval, modifier);
  if (command == -1)
    command = GetCustomCommandId(keyval, modifier);
  return command;
}

void BrowserWindowGtk::ShowCreateShortcutsDialog(TabContents* tab_contents) {
  SkBitmap bitmap;
  if (tab_contents->FavIconIsValid())
    bitmap = tab_contents->GetFavIcon();
  CreateApplicationShortcutsDialogGtk::Show(window_,
                                            tab_contents->GetURL(),
                                            tab_contents->GetTitle(),
                                            bitmap);
}

void BrowserWindowGtk::ConfirmBrowserCloseWithPendingDownloads() {
  new DownloadInProgressDialogGtk(browser());
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
      browser_->GetFindBarController()->find_bar()->SetFocusAndSelection();
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

void BrowserWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  // Do nothing if we're in the process of closing the browser window.
  if (!window_)
    return;

  bool is_active = (GTK_WIDGET(window_)->window == active_window);
  bool changed = (is_active != is_active_);

  if (is_active && changed) {
    // If there's an app modal dialog (e.g., JS alert), try to redirect
    // the user's attention to the window owning the dialog.
    if (Singleton<AppModalDialogQueue>()->HasActiveDialog()) {
      Singleton<AppModalDialogQueue>()->ActivateModalDialog();
      return;
    }
  }

  is_active_ = is_active;
  if (changed) {
    SetBackgroundColor();
    gdk_window_invalidate_rect(GTK_WIDGET(window_)->window,
                               &GTK_WIDGET(window_)->allocation, TRUE);
    // For some reason, the above two calls cause the window shape to be
    // lost so reset it.
    UpdateWindowShape(bounds_.width(), bounds_.height());
  }
}

void BrowserWindowGtk::MaybeShowBookmarkBar(TabContents* contents,
                                            bool animate) {
  if (!IsBookmarkBarSupported())
    return;

  bool show_bar = false;

  if (IsBookmarkBarSupported() && contents) {
    bookmark_bar_->SetProfile(contents->profile());
    bookmark_bar_->SetPageNavigator(contents);
    show_bar = true;
  }

  if (show_bar && contents && !contents->ShouldShowBookmarkBar()) {
    PrefService* prefs = contents->profile()->GetPrefs();
    show_bar = prefs->GetBoolean(prefs::kShowBookmarkBar) && !IsFullscreen();
  }

  if (show_bar) {
    bookmark_bar_->Show(animate);
  } else if (IsFullscreen()) {
    bookmark_bar_->EnterFullscreen();
  } else {
    bookmark_bar_->Hide(animate);
  }
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
  GetLocationBar()->location_entry()->ClosePopup();

  if (bounds_.size() != bounds.size())
    OnSizeChanged(bounds.width(), bounds.height());

  // We update |bounds_| but not |restored_bounds_| here.  The latter needs
  // to be updated conditionally when the window is non-maximized and non-
  // fullscreen, but whether those state updates have been processed yet is
  // window-manager specific.  We update |restored_bounds_| in the debounced
  // handler below, after the window state has been updated.
  bounds_ = bounds;

  // When a window is moved or resized, GTK will call MainWindowConfigured()
  // above.  The GdkEventConfigure* that it gets doesn't have quite the right
  // coordinates though (they're relative to the drawable window area, rather
  // than any window manager decorations, if enabled), so we need to call
  // gtk_window_get_position() to get the right values.  (Otherwise session
  // restore, if enabled, will restore windows to incorrect positions.)  That's
  // a round trip to the X server though, so we set a debounce timer and only
  // call it (in OnDebouncedBoundsChanged() below) after we haven't seen a
  // reconfigure event in a short while.
  // We don't use Reset() because the timer may not yet be running.
  // (In that case Stop() is a no-op.)
  window_configure_debounce_timer_.Stop();
  window_configure_debounce_timer_.Start(base::TimeDelta::FromMilliseconds(
      kDebounceTimeoutMilliseconds), this,
      &BrowserWindowGtk::OnDebouncedBoundsChanged);
}

void BrowserWindowGtk::OnDebouncedBoundsChanged() {
  gint x, y;
  gtk_window_get_position(window_, &x, &y);
  gfx::Point origin(x, y);
  bounds_.set_origin(origin);
  if (!IsFullscreen() && !IsMaximized())
    restored_bounds_ = bounds_;
  SaveWindowPosition();
}

void BrowserWindowGtk::OnStateChanged(GdkWindowState state,
                                      GdkWindowState changed_mask) {
  state_ = state;

  if (changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    bool is_fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
    browser_->UpdateCommandsForFullscreenMode(is_fullscreen);
    if (is_fullscreen) {
      UpdateCustomFrame();
      toolbar_->Hide();
      tabstrip_->Hide();
      if (IsBookmarkBarSupported())
        bookmark_bar_->EnterFullscreen();
      bool is_kiosk =
          CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
      if (!is_kiosk) {
        fullscreen_exit_bubble_.reset(new FullscreenExitBubbleGtk(
            GTK_FLOATING_CONTAINER(render_area_floating_container_)));
      }
      gtk_widget_hide(toolbar_border_);
#if defined(OS_CHROMEOS)
      if (main_menu_button_)
        gtk_widget_hide(main_menu_button_->widget());
      if (compact_navbar_hbox_)
        gtk_widget_hide(compact_navbar_hbox_);
      if (status_area_)
        status_area_->GetWidget()->Hide();
#endif
    } else {
      fullscreen_exit_bubble_.reset();
      UpdateCustomFrame();
      ShowSupportedWindowFeatures();

      gtk_widget_show(toolbar_border_);
      gdk_window_lower(toolbar_border_->window);
    }
  }

  UpdateWindowShape(bounds_.width(), bounds_.height());
  SaveWindowPosition();
}

void BrowserWindowGtk::UnMaximize() {
  // It can happen that you end up with a window whose restore size is the same
  // as the size of the screen, so unmaximizing it merely remaximizes it due to
  // the same WM feature that SetWindowSize() works around.  We try to detect
  // this and resize the window to work around the issue.
  if (bounds_.size() == restored_bounds_.size())
    gtk_window_resize(window_, bounds_.width(), bounds_.height() - 1);
  else
    gtk_window_unmaximize(window_);
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
  gtk_floating_container_add_floating(
      GTK_FLOATING_CONTAINER(render_area_floating_container_),
      findbar->widget());
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
  std::map<XID, GtkWindow*>::iterator iter =
      BrowserWindowGtk::xid_map_.find(xid);
  return (iter != BrowserWindowGtk::xid_map_.end()) ? iter->second : NULL;
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

void BrowserWindowGtk::BookmarkBarIsFloating(bool is_floating) {
  toolbar_->UpdateForBookmarkBarVisibility(is_floating);

  // This can be NULL during initialization of the bookmark bar.
  if (bookmark_bar_.get())
    PlaceBookmarkBar(is_floating);
}

void BrowserWindowGtk::SetGeometryHints() {
  // Do not allow the user to resize us arbitrarily small. When using the
  // custom frame, the window can disappear entirely while resizing, or get
  // to a size that's very hard to resize.
  GdkGeometry geometry;
  geometry.min_width = 100;
  geometry.min_height = 100;
  gtk_window_set_geometry_hints(window_, NULL, &geometry, GDK_HINT_MIN_SIZE);

  // If we call gtk_window_maximize followed by gtk_window_present, compiz gets
  // confused and maximizes the window, but doesn't set the
  // GDK_WINDOW_STATE_MAXIMIZED bit.  So instead, we keep track of whether to
  // maximize and call it after gtk_window_present.
  maximize_after_show_ = browser_->GetSavedMaximizedState();

  gfx::Rect bounds = browser_->GetSavedWindowBounds();
  // We don't blindly call SetBounds here: that sets a forced position
  // on the window and we intentionally *don't* do that for normal
  // windows.  Most programs do not restore their window position on
  // Linux, instead letting the window manager choose a position.
  //
  // However, in cases like dropping a tab where the bounds are
  // specifically set, we do want to position explicitly.  We also
  // force the position as part of session restore, as applications
  // that restore other, similar state (for instance GIMP, audacity,
  // pidgin, dia, and gkrellm) do tend to restore their positions.
  if (browser_->bounds_overridden()) {
    // For popups, bounds are set in terms of the client area rather than the
    // entire window.
    SetBoundsImpl(bounds, !(browser_->type() & Browser::TYPE_POPUP));
  } else {
    // Ignore the position but obey the size.
    SetWindowSize(window_, bounds.width(), bounds.height());
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
  ConnectHandlersToSignals();
  bounds_ = restored_bounds_ = GetInitialWindowBounds(window_);

  // This vbox encompasses all of the widgets within the browser.  This is
  // everything except the custom frame border.
  window_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(window_vbox_);

  // The window container draws the custom browser frame.
  window_container_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_widget_set_name(window_container_, "chrome-custom-frame-border");
  gtk_widget_set_app_paintable(window_container_, TRUE);
  gtk_widget_set_double_buffered(window_container_, FALSE);
  gtk_widget_set_redraw_on_allocate(window_container_, TRUE);
  g_signal_connect(G_OBJECT(window_container_), "expose-event",
                   G_CALLBACK(&OnCustomFrameExpose), this);
  gtk_container_add(GTK_CONTAINER(window_container_), window_vbox_);

  tabstrip_.reset(new TabStripGtk(browser_->tabstrip_model(), this));
  tabstrip_->Init();

  // Build the titlebar (tabstrip + header space + min/max/close buttons).
  titlebar_.reset(new BrowserTitlebar(this, window_));

#if defined(OS_CHROMEOS)
  GtkWidget* titlebar_hbox = NULL;
  GtkWidget* status_container = NULL;
  bool has_compact_nav_bar = next_window_should_use_compact_nav_;
  if (browser_->type() == Browser::TYPE_NORMAL) {
    // Make a box that we'll later insert the compact navigation bar into. The
    // tabstrip must go into an hbox with our box so that they can get arranged
    // horizontally.
    titlebar_hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(titlebar_hbox);

    // Main menu button.
    main_menu_button_ =
        new CustomDrawButton(IDR_MAIN_MENU_BUTTON, IDR_MAIN_MENU_BUTTON,
                             IDR_MAIN_MENU_BUTTON, 0);
    gtk_widget_show(main_menu_button_->widget());
    g_signal_connect(G_OBJECT(main_menu_button_->widget()), "clicked",
                     G_CALLBACK(OnMainMenuButtonClicked), this);
    GTK_WIDGET_UNSET_FLAGS(main_menu_button_->widget(), GTK_CAN_FOCUS);
    gtk_box_pack_start(GTK_BOX(titlebar_hbox), main_menu_button_->widget(),
                       FALSE, FALSE, 0);

    chromeos::MainMenu::ScheduleCreation();

    if (has_compact_nav_bar) {
      compact_navbar_hbox_ = gtk_hbox_new(FALSE, 0);
      gtk_widget_show(compact_navbar_hbox_);
      gtk_box_pack_start(GTK_BOX(titlebar_hbox), compact_navbar_hbox_,
                         FALSE, FALSE, 0);

      // Reset the compact nav bit now that we're creating the next toplevel
      // window. Code below will use our local has_compact_nav_bar variable.
      next_window_should_use_compact_nav_ = false;
    }
    status_container = gtk_fixed_new();
    gtk_widget_show(status_container);
    gtk_box_pack_start(GTK_BOX(titlebar_hbox), titlebar_->widget(), TRUE, TRUE,
                       0);
    gtk_box_pack_start(GTK_BOX(titlebar_hbox), status_container, FALSE, FALSE,
                       0);
    gtk_box_pack_start(GTK_BOX(window_vbox_), titlebar_hbox, FALSE, FALSE, 0);
  }

#else
  // Insert the tabstrip into the window.
  gtk_box_pack_start(GTK_BOX(window_vbox_), titlebar_->widget(), FALSE, FALSE,
                     0);
#endif  // OS_CHROMEOS

  toolbar_.reset(new BrowserToolbarGtk(browser_.get(), this));
  toolbar_->Init(browser_->profile(), window_);
  gtk_box_pack_start(GTK_BOX(window_vbox_), toolbar_->widget(),
                     FALSE, FALSE, 0);
#if defined(OS_CHROMEOS)
  if (browser_->type() == Browser::TYPE_NORMAL && has_compact_nav_bar) {
    gtk_widget_hide(toolbar_->widget());

    GtkWidget* spacer = gtk_vbox_new(FALSE, 0);
    gtk_widget_set_size_request(spacer, -1, 3);
    gtk_widget_show(spacer);
    gtk_box_pack_start(GTK_BOX(window_vbox_), spacer, FALSE, FALSE, 0);
    g_signal_connect(spacer, "expose-event",
                     G_CALLBACK(&OnCompactNavSpacerExpose), this);
  }
#endif

  // This vbox surrounds the render area: find bar, info bars and render view.
  // The reason is that this area as a whole needs to be grouped in its own
  // GdkWindow hierarchy so that animations originating inside it (infobar,
  // download shelf, find bar) are all clipped to that area. This is why
  // |render_area_vbox_| is packed in |render_area_event_box_|.
  render_area_vbox_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_set_name(render_area_vbox_, "chrome-render-area-vbox");
  render_area_floating_container_ = gtk_floating_container_new();
  gtk_container_add(GTK_CONTAINER(render_area_floating_container_),
                    render_area_vbox_);

  toolbar_border_ = gtk_event_box_new();
  gtk_box_pack_start(GTK_BOX(render_area_vbox_),
                     toolbar_border_, FALSE, FALSE, 0);
  gtk_widget_set_size_request(toolbar_border_, -1, 1);
  gtk_widget_show(toolbar_border_);

  infobar_container_.reset(new InfoBarContainerGtk(browser_->profile()));
  gtk_box_pack_start(GTK_BOX(render_area_vbox_),
                     infobar_container_->widget(),
                     FALSE, FALSE, 0);

  status_bubble_.reset(new StatusBubbleGtk(browser_->profile()));

  contents_container_.reset(new TabContentsContainerGtk(status_bubble_.get()));
  devtools_container_.reset(new TabContentsContainerGtk(NULL));
  ViewIDUtil::SetID(devtools_container_->widget(), VIEW_ID_DEV_TOOLS_DOCKED);
  contents_split_ = gtk_vpaned_new();
  gtk_paned_pack1(GTK_PANED(contents_split_), contents_container_->widget(),
                  TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(contents_split_), devtools_container_->widget(),
                  FALSE, TRUE);
  gtk_box_pack_end(GTK_BOX(render_area_vbox_), contents_split_, TRUE, TRUE, 0);
  // Restore split offset.
  int split_offset = g_browser_process->local_state()->GetInteger(
      prefs::kDevToolsSplitLocation);
  if (split_offset != -1) {
    gtk_paned_set_position(GTK_PANED(contents_split_), split_offset);
  } else {
    gtk_widget_set_size_request(devtools_container_->widget(), -1,
                                kDefaultDevToolsHeight);
  }
  gtk_widget_show_all(render_area_floating_container_);
  gtk_widget_hide(devtools_container_->widget());
  render_area_event_box_ = gtk_event_box_new();
  // Set a white background so during startup the user sees white in the
  // content area before we get a TabContents in place.
  gtk_widget_modify_bg(render_area_event_box_, GTK_STATE_NORMAL,
                       &gfx::kGdkWhite);
  gtk_container_add(GTK_CONTAINER(render_area_event_box_),
                    render_area_floating_container_);
  gtk_widget_show(render_area_event_box_);
  gtk_box_pack_end(GTK_BOX(window_vbox_), render_area_event_box_,
                   TRUE, TRUE, 0);

  if (IsBookmarkBarSupported()) {
    bookmark_bar_.reset(new BookmarkBarGtk(this,
                                           browser_->profile(),
                                           browser_.get(),
                                           tabstrip_.get()));
    PlaceBookmarkBar(false);
    gtk_widget_show(bookmark_bar_->widget());
  }

#if defined(OS_CHROMEOS)
  if (browser_->type() & Browser::TYPE_POPUP) {
    toolbar_->Hide();
    // The window manager needs the min size for popups.
    gtk_widget_set_size_request(
        GTK_WIDGET(window_), bounds_.width(), bounds_.height());
    // If we don't explicitly resize here there is a race condition between
    // the X Server and the window manager. Windows will appear with a default
    // size of 200x200 if this happens.
    gtk_window_resize(window_, bounds_.width(), bounds_.height());
  }
#endif

  // We have to realize the window before we try to apply a window shape mask.
  gtk_widget_realize(GTK_WIDGET(window_));
  state_ = gdk_window_get_state(GTK_WIDGET(window_)->window);
  // Note that calling this the first time is necessary to get the
  // proper control layout.
  UpdateCustomFrame();

  gtk_container_add(GTK_CONTAINER(window_), window_container_);
  gtk_widget_show(window_container_);
  browser_->tabstrip_model()->AddObserver(this);

#if defined(OS_CHROMEOS)
  if (browser_->type() == Browser::TYPE_NORMAL) {
    if (has_compact_nav_bar) {
      // Create the compact navigation bar. This must be done after adding
      // everything to the window since it's done in Views, which expects to
      // call realize (requiring a window) in the Init function.
      views::WidgetGtk* clb_widget =
          new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
      clb_widget->set_delete_on_destroy(true);
      // Must initialize with a NULL parent since the widget will assume the
      // parent is also a WidgetGtk. Then we can parent the native widget
      // afterwards.
      clb_widget->Init(NULL, gfx::Rect(0, 0, 100, 30));
      gtk_widget_reparent(clb_widget->GetNativeView(), compact_navbar_hbox_);

      compact_navigation_bar_ =
          new chromeos::CompactNavigationBar(browser_.get());

      clb_widget->SetContentsView(compact_navigation_bar_);
      compact_navigation_bar_->Init();

      // Must be after Init.
      gtk_widget_set_size_request(clb_widget->GetNativeView(),
          compact_navigation_bar_->GetPreferredSize().width(), 20);
      clb_widget->Show();
    }

    // Create the status area.
    views::WidgetGtk* status_widget =
        new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
    status_widget->set_delete_on_destroy(true);
    status_widget->Init(NULL, gfx::Rect(0, 0, 100, 30));
    gtk_widget_reparent(status_widget->GetNativeView(), status_container);
    status_area_ = new chromeos::StatusAreaView(browser(), GetNativeHandle());
    status_widget->SetContentsView(status_area_);
    status_area_->Init();

    // Must be after Init.
    gfx::Size status_area_size = status_area_->GetPreferredSize();
    gtk_widget_set_size_request(status_widget->GetNativeView(),
                                status_area_size.width(),
                                status_area_size.height());
    status_widget->Show();
  }
#endif  // OS_CHROMEOS
}

void BrowserWindowGtk::SetBackgroundColor() {
  Profile* profile = browser()->profile();
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(profile);
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
  GdkColor frame_color_gdk = SkColorToGdkColor(frame_color);
  gtk_widget_modify_bg(GTK_WIDGET(window_), GTK_STATE_NORMAL,
                       &frame_color_gdk);

  // Set the color of the dev tools divider.
  gtk_widget_modify_bg(contents_split_, GTK_STATE_NORMAL, &frame_color_gdk);

  // When the cursor is over the divider, GTK+ normally lightens the background
  // color by 1.3 (see LIGHTNESS_MULT in gtkstyle.c).  Since we're setting the
  // color, override the prelight also.
  color_utils::HSL hsl = { -1, 0.5, 0.65 };
  SkColor frame_prelight_color = color_utils::HSLShift(frame_color, hsl);
  GdkColor frame_prelight_color_gdk = SkColorToGdkColor(frame_prelight_color);
  gtk_widget_modify_bg(contents_split_, GTK_STATE_PRELIGHT,
      &frame_prelight_color_gdk);

  GdkColor border_color = theme_provider->GetBorderColor();
  gtk_widget_modify_bg(toolbar_border_, GTK_STATE_NORMAL, &border_color);
}

void BrowserWindowGtk::OnSizeChanged(int width, int height) {
  UpdateWindowShape(width, height);
}

void BrowserWindowGtk::UpdateWindowShape(int width, int height) {
  if (UseCustomFrame() && !IsFullscreen() && !IsMaximized()) {
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
    if (UseCustomFrame()) {
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
  gtk_window_set_decorated(window_, !UseCustomFrame());
  titlebar_->UpdateCustomFrame(UseCustomFrame() && !IsFullscreen());
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
  int command_id = ::GetCommandId(keyval, modifier);
  DCHECK_NE(command_id, -1);
  browser_window->ExecuteBrowserCommand(command_id);

  return TRUE;
}

// static
gboolean BrowserWindowGtk::OnMouseMoveEvent(GtkWidget* widget,
    GdkEventMotion* event, BrowserWindowGtk* browser) {
  // This method is used to update the mouse cursor when over the edge of the
  // custom frame.  If the custom frame is off or we're over some other widget,
  // do nothing.
  if (!browser->UseCustomFrame() || event->window != widget->window) {
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
      browser->frame_cursor_ = gtk_util::GetCursor(new_cursor);
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

  // Make the button press coordinate relative to the browser window.
  int win_x, win_y;
  gdk_window_get_origin(GTK_WIDGET(browser->window_)->window, &win_x, &win_y);

  GdkWindowEdge edge;
  gfx::Point point(static_cast<int>(event->x_root - win_x),
                   static_cast<int>(event->y_root - win_y));
  bool has_hit_edge = browser->GetWindowEdge(point.x(), point.y(), &edge);

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
      browser->last_click_position_ = gfx::Point(static_cast<int>(event->x),
                                                 static_cast<int>(event->y));

      // Raise the window after a click on either the titlebar or the border to
      // match the behavior of most window managers.
      if (has_hit_titlebar || has_hit_edge)
        gdk_window_raise(GTK_WIDGET(browser->window_)->window);

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
        int click_move_x = static_cast<int>(event->x - last_click_position.x());
        int click_move_y = static_cast<int>(event->y - last_click_position.y());

        if (click_time > static_cast<guint32>(double_click_time) ||
            click_move_x > double_click_distance ||
            click_move_y > double_click_distance) {
          gtk_window_begin_move_drag(browser->window_, event->button,
                                     static_cast<gint>(event->x_root),
                                     static_cast<gint>(event->y_root),
                                     event->time);
          return TRUE;
        }
      } else if (has_hit_edge) {
        gtk_window_begin_resize_drag(browser->window_, edge, event->button,
                                     static_cast<gint>(event->x_root),
                                     static_cast<gint>(event->y_root),
                                     event->time);
        return TRUE;
      }
    } else if (GDK_2BUTTON_PRESS == event->type) {
      if (has_hit_titlebar) {
        // Maximize/restore on double click.
        if (browser->IsMaximized()) {
          browser->UnMaximize();
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

#if defined(OS_CHROMEOS)
  if (main_menu_button_)
    gtk_widget_show(main_menu_button_->widget());

  if (compact_navbar_hbox_)
    gtk_widget_show(compact_navbar_hbox_);

  if (status_area_)
    status_area_->GetWidget()->Show();
#endif
}

void BrowserWindowGtk::HideUnsupportedWindowFeatures() {
  if (!IsTabStripSupported())
    tabstrip_->Hide();

  if (!IsToolbarSupported())
    toolbar_->Hide();

  // If the bookmark bar shelf is unsupported, then we never create it.
}

bool BrowserWindowGtk::IsTabStripSupported() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserWindowGtk::IsToolbarSupported() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

bool BrowserWindowGtk::IsBookmarkBarSupported() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR);
}

bool BrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  if (!UseCustomFrame())
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

bool BrowserWindowGtk::UseCustomFrame() {
  // We don't use the custom frame for app mode windows.
  return use_custom_frame_pref_.GetValue() &&
      (browser_->type() != Browser::TYPE_APP);
}

void BrowserWindowGtk::PlaceBookmarkBar(bool is_floating) {
  GtkWidget* parent = gtk_widget_get_parent(bookmark_bar_->widget());
  if (parent)
    gtk_container_remove(GTK_CONTAINER(parent), bookmark_bar_->widget());

  if (!is_floating) {
    // Place the bookmark bar at the end of |window_vbox_|; this happens after
    // we have placed the render area at the end of |window_vbox_| so we will
    // be above the render area.
    gtk_box_pack_end(GTK_BOX(window_vbox_), bookmark_bar_->widget(),
                     FALSE, FALSE, 0);
  } else {
    // Place the bookmark bar at the end of the render area; this happens after
    // the tab contents container has been placed there so we will be
    // above the webpage (in terms of y).
    gtk_box_pack_end(GTK_BOX(render_area_vbox_), bookmark_bar_->widget(),
                     FALSE, FALSE, 0);
  }
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
  // system decorations for the few (?) tiling window managers where the custom
  // frame doesn't make sense (e.g. awesome, ion3, ratpoison, xmonad, etc.) or
  // other WMs where it has issues (e.g. Fluxbox -- see issue 19130).  The EWMH
  // _NET_SUPPORTING_WM property makes it easy to look up a name for the current
  // WM, but at least some of the WMs in the latter group don't set it.
  // Instead, we default to using system decorations for all WMs and
  // special-case the ones where the custom frame should be used.  These names
  // are taken from the WMs' source code.
  return (wm_name == "Blackbox" ||
          wm_name == "compiz" ||
          wm_name == "e16" ||  // Enlightenment DR16
          wm_name == "KWin" ||
          wm_name == "Metacity" ||
          wm_name == "Mutter" ||
          wm_name == "Openbox" ||
          wm_name == "Xfwm4");
}
