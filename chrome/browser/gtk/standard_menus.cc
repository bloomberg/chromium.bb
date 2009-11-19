// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/standard_menus.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/profile.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

struct MenuCreateMaterial zoom_menu_materials[] = {
  { MENU_NORMAL, IDC_ZOOM_PLUS, IDS_ZOOM_PLUS, 0, NULL,
    GDK_KP_Add, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_ZOOM_NORMAL, IDS_ZOOM_NORMAL, 0, NULL,
    GDK_KP_0, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_ZOOM_MINUS, IDS_ZOOM_MINUS, 0, NULL,
    GDK_KP_Subtract, GDK_CONTROL_MASK },
  { MENU_END }
};

struct MenuCreateMaterial developer_menu_materials[] = {
  { MENU_NORMAL, IDC_VIEW_SOURCE, IDS_VIEW_SOURCE, 0, NULL,
    GDK_u, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_DEV_TOOLS, IDS_DEV_TOOLS, 0, NULL,
    GDK_i, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_NORMAL, IDC_DEV_TOOLS_CONSOLE, IDS_DEV_TOOLS_CONSOLE, 0, NULL,
    GDK_j, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_NORMAL, IDC_TASK_MANAGER, IDS_TASK_MANAGER, 0, NULL,
    GDK_Escape, GDK_SHIFT_MASK },
  { MENU_END }
};

struct MenuCreateMaterial developer_menu_materials_no_inspector[] = {
  { MENU_NORMAL, IDC_VIEW_SOURCE, IDS_VIEW_SOURCE, 0, NULL,
    GDK_u, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_TASK_MANAGER, IDS_TASK_MANAGER, 0, NULL,
    GDK_Escape, GDK_SHIFT_MASK },
  { MENU_END }
};

struct MenuCreateMaterial standard_page_menu_materials[] = {
  { MENU_NORMAL, IDC_CREATE_SHORTCUTS, IDS_CREATE_SHORTCUTS },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_CUT, IDS_CUT, 0, NULL, GDK_x, GDK_CONTROL_MASK, true },
  { MENU_NORMAL, IDC_COPY, IDS_COPY, 0, NULL, GDK_c, GDK_CONTROL_MASK, true },
  { MENU_NORMAL, IDC_PASTE, IDS_PASTE, 0, NULL, GDK_v, GDK_CONTROL_MASK, true },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_FIND, IDS_FIND, 0, NULL, GDK_f, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_SAVE_PAGE, IDS_SAVE_PAGE, 0, NULL, GDK_s,
    GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_PRINT, IDS_PRINT, 0, NULL, GDK_p, GDK_CONTROL_MASK },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_ZOOM_MENU, IDS_ZOOM_MENU, 0, zoom_menu_materials },
  // The encoding menu submenu is filled in by code below.
  { MENU_NORMAL, IDC_ENCODING_MENU, IDS_ENCODING_MENU },
  { MENU_SEPARATOR },
  // The developer menu submenu is filled in by code below.
  { MENU_NORMAL, IDC_DEVELOPER_MENU, IDS_DEVELOPER_MENU },

  // The Report Bug menu hasn't been implemented yet.  Remove it until it is.
  // http://code.google.com/p/chromium/issues/detail?id=11600
  // { MENU_SEPARATOR },
  // { MENU_NORMAL, IDC_REPORT_BUG, IDS_REPORT_BUG },

  { MENU_END }
};

// -----------------------------------------------------------------------

struct MenuCreateMaterial standard_app_menu_materials[] = {
  { MENU_NORMAL, IDC_NEW_TAB, IDS_NEW_TAB, 0, NULL,
    GDK_t, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_NEW_WINDOW, IDS_NEW_WINDOW, 0, NULL,
    GDK_n, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW, 0, NULL,
    GDK_n, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_SEPARATOR },
  { MENU_CHECKBOX, IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR, 0, NULL,
    GDK_b, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_FULLSCREEN, IDS_FULLSCREEN, 0, NULL, GDK_F11 },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_SHOW_HISTORY, IDS_SHOW_HISTORY, 0, NULL,
    GDK_h, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER, 0, NULL,
    GDK_b, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_NORMAL, IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS, 0, NULL,
    GDK_j, GDK_CONTROL_MASK },
  { MENU_NORMAL, IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_SYNC_BOOKMARKS, IDS_SYNC_MY_BOOKMARKS_LABEL},
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_OPTIONS, IDS_OPTIONS, IDS_PRODUCT_NAME },
  { MENU_NORMAL, IDC_ABOUT, IDS_ABOUT, IDS_PRODUCT_NAME },
  { MENU_NORMAL, IDC_HELP_PAGE, IDS_HELP_PAGE, 0, NULL, GDK_F1 },
#if !defined(OS_CHROMEOS)
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_EXIT, IDS_EXIT, 0, NULL, GDK_q,
    GDK_CONTROL_MASK | GDK_SHIFT_MASK },
#endif
  { MENU_END }
};

}  // namespace

const MenuCreateMaterial* GetStandardPageMenu(Profile* profile,
                                              MenuGtk::Delegate* delegate) {
  EncodingMenuController controller;
  EncodingMenuController::EncodingMenuItemList items;
  controller.GetEncodingMenuItems(profile, &items);

  MenuGtk* encodings_menu = new MenuGtk(delegate, false);
  GSList* radio_group = NULL;
  for (EncodingMenuController::EncodingMenuItemList::const_iterator i =
           items.begin();
       i != items.end(); ++i) {
    if (i == items.begin()) {
      encodings_menu->AppendCheckMenuItemWithLabel(i->first,
                                                   UTF16ToUTF8(i->second));
    } else if (i->first == 0) {
      encodings_menu->AppendSeparator();
    } else {
      GtkWidget* item =
          gtk_radio_menu_item_new_with_label(radio_group,
                                             UTF16ToUTF8(i->second).c_str());
      radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      encodings_menu->AppendMenuItem(i->first, item);
    }
  }

  // Find the encoding menu and attach this menu.
  for (MenuCreateMaterial* entry = standard_page_menu_materials;
       entry->type != MENU_END; ++entry) {
    if (entry->id == IDC_ENCODING_MENU) {
      entry->custom_submenu = encodings_menu;
    } else if (entry->id == IDC_DEVELOPER_MENU) {
      entry->submenu = g_browser_process->have_inspector_files() ?
        developer_menu_materials : developer_menu_materials_no_inspector;
    }
  }

  return standard_page_menu_materials;
}

const MenuCreateMaterial* GetStandardAppMenu() {
  return standard_app_menu_materials;
}
