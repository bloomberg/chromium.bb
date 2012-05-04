// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/global_menu_bar.h"

#include <gtk/gtk.h>

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/accelerators_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"

struct GlobalMenuBarCommand {
  int str_id;
  int command;
  int tag;
};

namespace {

const int MENU_SEPARATOR =-1;
const int MENU_END = -2;
const int MENU_DISABLED_LABEL = -3;

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

GlobalMenuBarCommand edit_menu[] = {
  { IDS_CUT, IDC_CUT },
  { IDS_COPY, IDC_COPY },
  { IDS_PASTE, IDC_PASTE },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_FIND, IDC_FIND },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_PREFERENCES, IDC_OPTIONS },

  { MENU_END, MENU_END }
};

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

GlobalMenuBarCommand history_menu[] = {
  { IDS_HISTORY_HOME_LINUX, IDC_HOME },
  { IDS_HISTORY_BACK_LINUX, IDC_BACK },
  { IDS_HISTORY_FORWARD_LINUX, IDC_FORWARD },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_HISTORY_VISITED_LINUX, MENU_DISABLED_LABEL,
    GlobalMenuBar::TAG_MOST_VISITED_HEADER },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_HISTORY_CLOSED_LINUX, MENU_DISABLED_LABEL,
    GlobalMenuBar::TAG_RECENTLY_CLOSED_HEADER },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_SHOWFULLHISTORY_LINK, IDC_SHOW_HISTORY },

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

GlobalMenuBar::GlobalMenuBar(Browser* browser)
    : browser_(browser),
      menu_bar_(gtk_menu_bar_new()),
      history_menu_(browser_),
      dummy_accel_group_(gtk_accel_group_new()),
      block_activation_(false) {
  // The global menu bar should never actually be shown in the app; it should
  // instead remain in our widget hierarchy simply to be noticed by third party
  // components.
  gtk_widget_set_no_show_all(menu_bar_.get(), TRUE);

  // Set a nice name so it shows up in gtkparasite and others.
  gtk_widget_set_name(menu_bar_.get(), "chrome-hidden-global-menubar");

  BuildGtkMenuFrom(IDS_FILE_MENU_LINUX, &id_to_menu_item_, file_menu, NULL);
  BuildGtkMenuFrom(IDS_EDIT_MENU_LINUX, &id_to_menu_item_, edit_menu, NULL);
  BuildGtkMenuFrom(IDS_VIEW_MENU_LINUX, &id_to_menu_item_, view_menu, NULL);
  BuildGtkMenuFrom(IDS_HISTORY_MENU_LINUX, &id_to_menu_item_,
                   history_menu, &history_menu_);

  BuildGtkMenuFrom(IDS_TOOLS_MENU_LINUX, &id_to_menu_item_, tools_menu, NULL);
  BuildGtkMenuFrom(IDS_HELP_MENU_LINUX, &id_to_menu_item_, help_menu, NULL);

  for (CommandIDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    // Get the starting enabled state.
    gtk_widget_set_sensitive(
        it->second,
        browser_->command_updater()->IsCommandEnabled(it->first));

    // Set the accelerator for each menu item.
    AcceleratorsGtk* accelerators = AcceleratorsGtk::GetInstance();
    const ui::AcceleratorGtk* accelerator =
        accelerators->GetPrimaryAcceleratorForCommand(it->first);
    if (accelerator) {
      gtk_widget_add_accelerator(it->second,
                                 "activate",
                                 dummy_accel_group_,
                                 accelerator->GetGdkKeyCode(),
                                 accelerator->gdk_modifier_type(),
                                 GTK_ACCEL_VISIBLE);
    }

    browser_->command_updater()->AddCommandObserver(it->first, this);
  }

  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(prefs::kShowBookmarkBar, this);
  OnBookmarkBarVisibilityChanged();
}

GlobalMenuBar::~GlobalMenuBar() {
  Disable();
  g_object_unref(dummy_accel_group_);
}

void GlobalMenuBar::Disable() {
  for (CommandIDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    browser_->command_updater()->RemoveCommandObserver(it->first, this);
  }
  id_to_menu_item_.clear();

  pref_change_registrar_.RemoveAll();
}

void GlobalMenuBar::BuildGtkMenuFrom(
    int menu_str_id,
    std::map<int, GtkWidget*>* id_to_menu_item,
    GlobalMenuBarCommand* commands,
    GlobalMenuOwner* owner) {
  GtkWidget* menu = gtk_menu_new();
  for (int i = 0; commands[i].str_id != MENU_END; ++i) {
    GtkWidget* menu_item = BuildMenuItem(
        commands[i].str_id, commands[i].command, commands[i].tag,
        id_to_menu_item, menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  }

  gtk_widget_show(menu);

  GtkWidget* menu_item = gtk_menu_item_new_with_mnemonic(
      ui::RemoveWindowsStyleAccelerators(
          l10n_util::GetStringUTF8(menu_str_id)).c_str());

  // Give the owner a chance to sink the reference before we add it to the menu
  // bar.
  if (owner)
    owner->Init(menu, menu_item);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar_.get()), menu_item);
}

GtkWidget* GlobalMenuBar::BuildMenuItem(
    int string_id,
    int command_id,
    int tag_id,
    std::map<int, GtkWidget*>* id_to_menu_item,
    GtkWidget* menu_to_add_to) {
  GtkWidget* menu_item = NULL;
  if (string_id == MENU_SEPARATOR) {
    menu_item = gtk_separator_menu_item_new();
  } else {
    std::string label = ui::ConvertAcceleratorsFromWindowsStyle(
        l10n_util::GetStringUTF8(string_id));

    if (command_id == IDC_SHOW_BOOKMARK_BAR)
      menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
    else
      menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());

    if (tag_id) {
      g_object_set_data(G_OBJECT(menu_item), "type-tag",
                        GINT_TO_POINTER(tag_id));
    }

    if (command_id == MENU_DISABLED_LABEL) {
      gtk_widget_set_sensitive(menu_item, FALSE);
    } else {
      id_to_menu_item->insert(std::make_pair(command_id, menu_item));
      g_object_set_data(G_OBJECT(menu_item), "command-id",
                        GINT_TO_POINTER(command_id));
      g_signal_connect(menu_item, "activate",
                       G_CALLBACK(OnItemActivatedThunk), this);
    }
  }
  gtk_widget_show(menu_item);
  return menu_item;
}

void GlobalMenuBar::EnabledStateChangedForCommand(int id, bool enabled) {
  CommandIDMenuItemMap::iterator it = id_to_menu_item_.find(id);
  if (it != id_to_menu_item_.end())
    gtk_widget_set_sensitive(it->second, enabled);
}

void GlobalMenuBar::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PREF_CHANGED, type);
  const std::string& pref_name = *content::Details<std::string>(details).ptr();
  DCHECK_EQ(prefs::kShowBookmarkBar, pref_name);
  OnBookmarkBarVisibilityChanged();
}

void GlobalMenuBar::OnBookmarkBarVisibilityChanged() {
  CommandIDMenuItemMap::iterator it =
      id_to_menu_item_.find(IDC_SHOW_BOOKMARK_BAR);
  if (it != id_to_menu_item_.end()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    block_activation_ = true;
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(it->second),
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
