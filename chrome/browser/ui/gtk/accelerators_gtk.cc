// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/accelerators_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include "base/memory/singleton.h"
#include "chrome/app/chrome_command_ids.h"

namespace {

// A mostly complete list of chrome's accelerators. When one command has
// multiple shortcuts, the first one in this list is considered "primary",
// meaning that it will be displayed in context menus.
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
  { GDK_w, IDC_CLOSE_TAB, GDK_CONTROL_MASK },
  { GDK_t, IDC_RESTORE_TAB,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_t, IDC_NEW_TAB, GDK_CONTROL_MASK },
  { GDK_n, IDC_NEW_WINDOW, GDK_CONTROL_MASK },
  { GDK_n, IDC_NEW_INCOGNITO_WINDOW,
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
  { XF86XK_Back, IDC_BACK, GdkModifierType(0) },
  { GDK_Right, IDC_FORWARD, GDK_MOD1_MASK },
  { XF86XK_Forward, IDC_FORWARD, GdkModifierType(0) },
  { GDK_r, IDC_RELOAD, GDK_CONTROL_MASK },
  { GDK_r, IDC_RELOAD_IGNORING_CACHE,
    GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK) },
  { GDK_F5, IDC_RELOAD, GdkModifierType(0) },
  { GDK_F5, IDC_RELOAD_IGNORING_CACHE, GDK_CONTROL_MASK },
  { GDK_F5, IDC_RELOAD_IGNORING_CACHE, GDK_SHIFT_MASK },
  { XF86XK_Reload, IDC_RELOAD, GdkModifierType(0) },
  { XF86XK_Refresh, IDC_RELOAD, GdkModifierType(0) },

  // Dev tools.
  { GDK_u, IDC_VIEW_SOURCE, GDK_CONTROL_MASK },
  { GDK_i, IDC_DEV_TOOLS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_F12, IDC_DEV_TOOLS, GdkModifierType(0) },
  { GDK_j, IDC_DEV_TOOLS_CONSOLE,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_c, IDC_DEV_TOOLS_INSPECT,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_Escape, IDC_TASK_MANAGER, GDK_SHIFT_MASK },

  // Editing.
  { GDK_c, IDC_COPY, GDK_CONTROL_MASK },
  { GDK_x, IDC_CUT, GDK_CONTROL_MASK },
  { GDK_v, IDC_PASTE, GDK_CONTROL_MASK },

  // Autofill.
  { GDK_a, IDC_AUTOFILL_DEFAULT,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },

  // Miscellany.
  { GDK_d, IDC_BOOKMARK_ALL_TABS,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_d, IDC_BOOKMARK_PAGE, GDK_CONTROL_MASK },
  { GDK_o, IDC_OPEN_FILE, GDK_CONTROL_MASK },
  { GDK_f, IDC_FIND, GDK_CONTROL_MASK },
  { GDK_p, IDC_PRINT, GDK_CONTROL_MASK },
  { GDK_b, IDC_SHOW_BOOKMARK_BAR,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_F11, IDC_FULLSCREEN, GdkModifierType(0) },
  { GDK_Delete, IDC_CLEAR_BROWSING_DATA,
    GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_h, IDC_SHOW_HISTORY, GDK_CONTROL_MASK },
  { GDK_j, IDC_SHOW_DOWNLOADS, GDK_CONTROL_MASK },
  { GDK_F1, IDC_HELP_PAGE, GdkModifierType(0) },
  { XF86XK_AddFavorite, IDC_BOOKMARK_PAGE, GdkModifierType(0) },
  { XF86XK_Favorites, IDC_SHOW_BOOKMARK_BAR, GdkModifierType(0) },
  { XF86XK_History, IDC_SHOW_HISTORY, GdkModifierType(0) },
  { GDK_q, IDC_EXIT, GdkModifierType(GDK_CONTROL_MASK | GDK_SHIFT_MASK) },
  { GDK_s, IDC_SAVE_PAGE, GDK_CONTROL_MASK },
  { GDK_e, IDC_SHOW_APP_MENU, GDK_MOD1_MASK },
  { GDK_f, IDC_SHOW_APP_MENU, GDK_MOD1_MASK },
};

}  // namespace

AcceleratorsGtk::AcceleratorsGtk() {
  for (size_t i = 0; i < arraysize(kAcceleratorMap); ++i) {
    int command_id = kAcceleratorMap[i].command_id;
    ui::AcceleratorGtk accelerator(kAcceleratorMap[i].keyval,
                                      kAcceleratorMap[i].modifier_type);
    all_accelerators_.push_back(
        std::pair<int, ui::AcceleratorGtk>(command_id, accelerator));

    if (primary_accelerators_.find(command_id) ==
        primary_accelerators_.end()) {
      primary_accelerators_[command_id] = accelerator;
    }
  }
}

AcceleratorsGtk::~AcceleratorsGtk() {}

// static
AcceleratorsGtk* AcceleratorsGtk::GetInstance() {
  return Singleton<AcceleratorsGtk>::get();
}

const ui::AcceleratorGtk* AcceleratorsGtk::GetPrimaryAcceleratorForCommand(
    int command_id) {
  base::hash_map<int, ui::AcceleratorGtk>::const_iterator iter =
      primary_accelerators_.find(command_id);

  if (iter == primary_accelerators_.end())
    return NULL;

  return &iter->second;
}
