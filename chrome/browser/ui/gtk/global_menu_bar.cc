// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/global_menu_bar.h"

#include <gtk/gtk.h>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/accelerators_gtk.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

struct GlobalMenuBarCommand {
  int str_id;
  int command;
};

namespace {

const int MENU_SEPARATOR =-1;
const int MENU_END = -2;

GlobalMenuBarCommand file_menu[] = {
  { IDS_NEW_TAB, IDC_NEW_TAB },
  { IDS_NEW_WINDOW, IDC_NEW_WINDOW },
  { IDS_NEW_INCOGNITO_WINDOW, IDC_NEW_INCOGNITO_WINDOW },
  { IDS_REOPEN_CLOSED_TABS_LINUX, IDC_RESTORE_TAB },
  { IDS_OPEN_FILE_LINUX, IDC_OPEN_FILE },
  { IDS_OPEN_LOCATION_LINUX, IDC_FOCUS_LOCATION },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_CREATE_SHORTCUTS, IDC_CREATE_SHORTCUTS },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_CLOSE_WINDOW_LINUX, IDC_CLOSE_WINDOW },
  { IDS_CLOSE_TAB_LINUX, IDC_CLOSE_TAB },
  { IDS_SAVE_PAGE, IDC_SAVE_PAGE },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_PRINT, IDC_PRINT },

  { MENU_END, MENU_END }
};

// TODO(erg): Need to add support for undo/redo/other editing commands that
// don't go through the command id framework.
GlobalMenuBarCommand edit_menu[] = {
  // TODO(erg): Undo
  // TODO(erg): Redo

  // TODO(erg): Separator

  { IDS_CUT, IDC_CUT },
  { IDS_COPY, IDC_COPY },
  { IDS_PASTE, IDC_PASTE },
  // TODO(erg): Delete

  { MENU_SEPARATOR, MENU_SEPARATOR },

  // TODO(erg): Select All
  // TODO(erg): Another separator

  { IDS_FIND, IDC_FIND },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_PREFERENCES, IDC_OPTIONS },

  { MENU_END, MENU_END }
};

// TODO(erg): The View menu should be overhauled and based on the Firefox view
// menu.
GlobalMenuBarCommand view_menu[] = {
  { IDS_SHOW_BOOKMARK_BAR, IDC_SHOW_BOOKMARK_BAR },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_STOP_MENU_LINUX, IDC_STOP },
  { IDS_RELOAD_MENU_LINUX, IDC_RELOAD },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_FULLSCREEN, IDC_FULLSCREEN },
  { IDS_TEXT_DEFAULT_LINUX, IDC_ZOOM_NORMAL },
  { IDS_TEXT_BIGGER_LINUX, IDC_ZOOM_PLUS },
  { IDS_TEXT_SMALLER_LINUX, IDC_ZOOM_MINUS },

  { MENU_END, MENU_END }
};

GlobalMenuBarCommand tools_menu[] = {
  { IDS_SHOW_DOWNLOADS, IDC_SHOW_DOWNLOADS },
  { IDS_SHOW_HISTORY, IDC_SHOW_HISTORY },
  { IDS_SHOW_EXTENSIONS, IDC_MANAGE_EXTENSIONS },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_TASK_MANAGER, IDC_TASK_MANAGER },
  { IDS_CLEAR_BROWSING_DATA, IDC_CLEAR_BROWSING_DATA },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_VIEW_SOURCE, IDC_VIEW_SOURCE },
  { IDS_DEV_TOOLS, IDC_DEV_TOOLS },
  { IDS_DEV_TOOLS_CONSOLE, IDC_DEV_TOOLS_CONSOLE },

  { MENU_END, MENU_END }
};

GlobalMenuBarCommand help_menu[] = {
  { IDS_FEEDBACK, IDC_FEEDBACK },
  { IDS_HELP_PAGE , IDC_HELP_PAGE },
  { MENU_END, MENU_END }
};

}  // namespace

GlobalMenuBar::GlobalMenuBar(Browser* browser,
                             BrowserWindowGtk* window)
    : browser_(browser),
      browser_window_(window),
      menu_bar_(gtk_menu_bar_new()),
      dummy_accel_group_(gtk_accel_group_new()),
      block_activation_(false) {
  // The global menu bar should never actually be shown in the app; it should
  // instead remain in our widget hierarchy simply to be noticed by third party
  // components.
  gtk_widget_set_no_show_all(menu_bar_, TRUE);

  // Set a nice name so it shows up in gtkparasite and others.
  gtk_widget_set_name(menu_bar_, "chrome-hidden-global-menubar");

  BuildGtkMenuFrom(IDS_FILE_MENU_LINUX, &id_to_menu_item_, file_menu);
  BuildGtkMenuFrom(IDS_EDIT_MENU_LINUX, &id_to_menu_item_, edit_menu);
  BuildGtkMenuFrom(IDS_VIEW_MENU_LINUX, &id_to_menu_item_, view_menu);
  BuildGtkMenuFrom(IDS_TOOLS_MENU_LINUX, &id_to_menu_item_, tools_menu);
  BuildGtkMenuFrom(IDS_HELP_MENU_LINUX, &id_to_menu_item_, help_menu);

  for (IDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    // Get the starting enabled state.
    gtk_widget_set_sensitive(
        it->second,
        browser_->command_updater()->IsCommandEnabled(it->first));

    // Set the accelerator for each menu item.
    const ui::AcceleratorGtk* accelerator_gtk =
        AcceleratorsGtk::GetInstance()->GetPrimaryAcceleratorForCommand(
            it->first);
    if (accelerator_gtk) {
      gtk_widget_add_accelerator(it->second,
                                 "activate",
                                 dummy_accel_group_,
                                 accelerator_gtk->GetGdkKeyCode(),
                                 accelerator_gtk->gdk_modifier_type(),
                                 GTK_ACCEL_VISIBLE);
    }

    browser_->command_updater()->AddCommandObserver(it->first, this);
  }

  // Listen for bookmark bar visibility changes and set the initial state.
  registrar_.Add(this, NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
                 NotificationService::AllSources());
  Observe(NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
          NotificationService::AllSources(),
          NotificationService::NoDetails());
}

GlobalMenuBar::~GlobalMenuBar() {
  for (IDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    browser_->command_updater()->RemoveCommandObserver(it->first, this);
  }

  g_object_unref(dummy_accel_group_);
}

void GlobalMenuBar::BuildGtkMenuFrom(int menu_str_id,
                                     std::map<int, GtkWidget*>* id_to_menu_item,
                                     GlobalMenuBarCommand* commands) {
  GtkWidget* menu = gtk_menu_new();
  for (int i = 0; commands[i].str_id != MENU_END; ++i) {
    GtkWidget* menu_item = NULL;
    if (commands[i].str_id == MENU_SEPARATOR) {
      menu_item = gtk_separator_menu_item_new();
    } else {
      int command_id = commands[i].command;
      std::string label =
          gfx::ConvertAcceleratorsFromWindowsStyle(
              l10n_util::GetStringUTF8(commands[i].str_id));

      if (command_id == IDC_SHOW_BOOKMARK_BAR)
        menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
      else
        menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());

      id_to_menu_item->insert(std::make_pair(command_id, menu_item));
      g_object_set_data(G_OBJECT(menu_item), "command-id",
                        GINT_TO_POINTER(command_id));
      g_signal_connect(menu_item, "activate",
                       G_CALLBACK(OnItemActivatedThunk), this);
    }
    gtk_widget_show(menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  }

  gtk_widget_show(menu);

  GtkWidget* menu_item = gtk_menu_item_new_with_mnemonic(
      gfx::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(menu_str_id)).c_str());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
  gtk_widget_show(menu_item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar_), menu_item);
}

void GlobalMenuBar::EnabledStateChangedForCommand(int id, bool enabled) {
  IDMenuItemMap::iterator it = id_to_menu_item_.find(id);
  if (it != id_to_menu_item_.end())
    gtk_widget_set_sensitive(it->second, enabled);
}

void GlobalMenuBar::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK(type.value == NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED);

  IDMenuItemMap::iterator it = id_to_menu_item_.find(IDC_SHOW_BOOKMARK_BAR);
  if (it != id_to_menu_item_.end()) {
    PrefService* prefs = browser_->profile()->GetPrefs();

    block_activation_ = true;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it->second),
                                   prefs->GetBoolean(prefs::kShowBookmarkBar));
    block_activation_ = false;
  }
}

void GlobalMenuBar::OnItemActivated(GtkWidget* sender) {
  if (block_activation_)
    return;

  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sender), "command-id"));
  browser_->ExecuteCommandIfEnabled(id);
}
