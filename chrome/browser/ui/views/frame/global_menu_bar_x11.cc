// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/global_menu_bar_x11.h"

#include <dlfcn.h>
#include <glib-object.h>

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host_x11.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/global_menu_bar_registrar_x11.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/l10n/l10n_util.h"

// libdbusmenu-glib types
typedef struct _DbusmenuMenuitem DbusmenuMenuitem;
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_new_func)();
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_new_with_id_func)(int id);

typedef int (*dbusmenu_menuitem_get_id_func)(DbusmenuMenuitem* item);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_child_append_func)(
    DbusmenuMenuitem* parent,
    DbusmenuMenuitem* child);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_func)(
    DbusmenuMenuitem* item,
    const char* property,
    const char* value);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_variant_func)(
    DbusmenuMenuitem* item,
    const char* property,
    GVariant* value);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_bool_func)(
    DbusmenuMenuitem* item,
    const char* property,
    bool value);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_int_func)(
    DbusmenuMenuitem* item,
    const char* property,
    int value);

typedef struct _DbusmenuServer      DbusmenuServer;
typedef DbusmenuServer* (*dbusmenu_server_new_func)(const char* object);
typedef void (*dbusmenu_server_set_root_func)(DbusmenuServer* self,
                                              DbusmenuMenuitem* root);

// A line in the static menu definitions.
struct GlobalMenuBarCommand {
  int str_id;
  int command;
  int tag;
};

namespace {

// Retrieved functions from libdbusmenu-glib.

// DbusmenuMenuItem methods:
dbusmenu_menuitem_new_func menuitem_new = NULL;
dbusmenu_menuitem_new_with_id_func menuitem_new_with_id = NULL;
dbusmenu_menuitem_get_id_func menuitem_get_id = NULL;
dbusmenu_menuitem_child_append_func menuitem_child_append = NULL;
dbusmenu_menuitem_property_set_func menuitem_property_set = NULL;
dbusmenu_menuitem_property_set_variant_func menuitem_property_set_variant =
    NULL;
dbusmenu_menuitem_property_set_bool_func menuitem_property_set_bool = NULL;
dbusmenu_menuitem_property_set_int_func menuitem_property_set_int = NULL;

// DbusmenuServer methods:
dbusmenu_server_new_func server_new = NULL;
dbusmenu_server_set_root_func server_set_root = NULL;

// Properties that we set on menu items:
const char kPropertyEnabled[] = "enabled";
const char kPropertyLabel[] = "label";
const char kPropertyShortcut[] = "shortcut";
const char kPropertyType[] = "type";
const char kPropertyToggleType[] = "toggle-type";
const char kPropertyToggleState[] = "toggle-state";
const char kPropertyVisible[] = "visible";

const char kTypeCheckmark[] = "checkmark";
const char kTypeSeparator[] = "separator";

// Constants used in menu definitions
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

// TODO(erg): History menu.

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
  { IDS_HELP_PAGE , IDC_HELP_PAGE_VIA_MENU },
  { MENU_END, MENU_END }
};


void EnsureMethodsLoaded() {
  static bool attempted_load = false;
  if (attempted_load)
    return;
  attempted_load = true;

  void* dbusmenu_lib = dlopen("libdbusmenu-glib.so", RTLD_LAZY);
  if (!dbusmenu_lib)
    return;

  // DbusmenuMenuItem methods.
  menuitem_new = reinterpret_cast<dbusmenu_menuitem_new_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_new"));
  menuitem_new_with_id = reinterpret_cast<dbusmenu_menuitem_new_with_id_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_new_with_id"));
  menuitem_get_id = reinterpret_cast<dbusmenu_menuitem_get_id_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_get_id"));
  menuitem_child_append = reinterpret_cast<dbusmenu_menuitem_child_append_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_child_append"));
  menuitem_property_set = reinterpret_cast<dbusmenu_menuitem_property_set_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set"));
  menuitem_property_set_variant =
      reinterpret_cast<dbusmenu_menuitem_property_set_variant_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set_variant"));
  menuitem_property_set_bool =
      reinterpret_cast<dbusmenu_menuitem_property_set_bool_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set_bool"));
  menuitem_property_set_int =
      reinterpret_cast<dbusmenu_menuitem_property_set_int_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set_int"));

  // DbusmenuServer methods.
  server_new = reinterpret_cast<dbusmenu_server_new_func>(
      dlsym(dbusmenu_lib, "dbusmenu_server_new"));
  server_set_root = reinterpret_cast<dbusmenu_server_set_root_func>(
      dlsym(dbusmenu_lib, "dbusmenu_server_set_root"));
}

}  // namespace

GlobalMenuBarX11::GlobalMenuBarX11(BrowserView* browser_view,
                                   BrowserDesktopRootWindowHostX11* host)
    : browser_(browser_view->browser()),
      browser_view_(browser_view),
      host_(host),
      server_(NULL),
      root_item_(NULL) {
  EnsureMethodsLoaded();

  if (server_new)
    host_->AddObserver(this);
}

GlobalMenuBarX11::~GlobalMenuBarX11() {
  if (server_) {
    Disable();
    g_object_unref(server_);
    host_->RemoveObserver(this);
  }
}

// static
std::string GlobalMenuBarX11::GetPathForWindow(unsigned long xid) {
  return base::StringPrintf("/com/canonical/menu/%lX", xid);
}

void GlobalMenuBarX11::InitServer(unsigned long xid) {
  std::string path = GetPathForWindow(xid);
  server_ = server_new(path.c_str());

  root_item_ = menuitem_new();
  menuitem_property_set(root_item_, kPropertyLabel, "Root");
  menuitem_property_set_bool(root_item_, kPropertyVisible, true);

  BuildMenuFrom(root_item_, IDS_FILE_MENU_LINUX, &id_to_menu_item_, file_menu);
  BuildMenuFrom(root_item_, IDS_EDIT_MENU_LINUX, &id_to_menu_item_, edit_menu);
  BuildMenuFrom(root_item_, IDS_VIEW_MENU_LINUX, &id_to_menu_item_, view_menu);
  // TODO(erg): History menu.
  BuildMenuFrom(root_item_, IDS_TOOLS_MENU_LINUX, &id_to_menu_item_,
                tools_menu);
  BuildMenuFrom(root_item_, IDS_HELP_MENU_LINUX, &id_to_menu_item_, help_menu);

  for (CommandIDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    menuitem_property_set_bool(it->second, kPropertyEnabled,
                               chrome::IsCommandEnabled(browser_, it->first));

    ui::Accelerator accelerator;
    if (browser_view_->GetAccelerator(it->first, &accelerator))
      RegisterAccelerator(it->second, accelerator);

    chrome::AddCommandObserver(browser_, it->first, this);
  }

  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kShowBookmarkBar,
      base::Bind(&GlobalMenuBarX11::OnBookmarkBarVisibilityChanged,
                 base::Unretained(this)));
  OnBookmarkBarVisibilityChanged();

  server_set_root(server_, root_item_);
}

void GlobalMenuBarX11::Disable() {
  for (CommandIDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    chrome::RemoveCommandObserver(browser_, it->first, this);
  }
  id_to_menu_item_.clear();

  pref_change_registrar_.RemoveAll();
}

void GlobalMenuBarX11::BuildMenuFrom(
    DbusmenuMenuitem* parent,
    int menu_str_id,
    std::map<int, DbusmenuMenuitem*>* id_to_menu_item,
    GlobalMenuBarCommand* commands) {
  DbusmenuMenuitem* top = menuitem_new();
  menuitem_property_set(
      top, kPropertyLabel,
      ui::RemoveWindowsStyleAccelerators(
          l10n_util::GetStringUTF8(menu_str_id)).c_str());
  menuitem_property_set_bool(top, kPropertyVisible, true);

  for (int i = 0; commands[i].str_id != MENU_END; ++i) {
    DbusmenuMenuitem* menu_item = BuildMenuItem(
        commands[i].str_id, commands[i].command, commands[i].tag,
        id_to_menu_item);
    menuitem_child_append(top, menu_item);
  }

  menuitem_child_append(parent, top);
}

DbusmenuMenuitem* GlobalMenuBarX11::BuildMenuItem(
    int string_id,
    int command_id,
    int tag_id,
    std::map<int, DbusmenuMenuitem*>* id_to_menu_item) {
  DbusmenuMenuitem* item = menuitem_new();

  if (string_id == MENU_SEPARATOR) {
    menuitem_property_set(item, kPropertyType, kTypeSeparator);
  } else {
    std::string label = ui::ConvertAcceleratorsFromWindowsStyle(
        l10n_util::GetStringUTF8(string_id));
    menuitem_property_set(item, kPropertyLabel, label.c_str());

    if (command_id == IDC_SHOW_BOOKMARK_BAR)
      menuitem_property_set(item, kPropertyToggleType, kTypeCheckmark);

    if (tag_id)
      g_object_set_data(G_OBJECT(item), "type-tag", GINT_TO_POINTER(tag_id));

    if (command_id == MENU_DISABLED_LABEL) {
      menuitem_property_set_bool(item, kPropertyEnabled, false);
    } else {
      id_to_menu_item->insert(std::make_pair(command_id, item));
      g_object_set_data(G_OBJECT(item), "command-id",
                        GINT_TO_POINTER(command_id));
      g_signal_connect(item, "item-activated",
                       G_CALLBACK(OnItemActivatedThunk), this);
    }
  }

  menuitem_property_set_bool(item, kPropertyVisible, true);
  return item;
}

void GlobalMenuBarX11::RegisterAccelerator(DbusmenuMenuitem* item,
                                           const ui::Accelerator& accelerator) {
  // A translation of libdbusmenu-gtk's menuitem_property_set_shortcut()
  // translated from GDK types to ui::Accelerator types.
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

  if (accelerator.IsCtrlDown())
    g_variant_builder_add(&builder, "s", "Control");
  if (accelerator.IsAltDown())
    g_variant_builder_add(&builder, "s", "Alt");
  if (accelerator.IsShiftDown())
    g_variant_builder_add(&builder, "s", "Shift");

  char* name = XKeysymToString(XKeysymForWindowsKeyCode(
      accelerator.key_code(), false));
  if (!name) {
    NOTIMPLEMENTED();
    return;
  }
  g_variant_builder_add(&builder, "s", name);

  GVariant* inside_array = g_variant_builder_end(&builder);
  g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add_value(&builder, inside_array);
  GVariant* outside_array = g_variant_builder_end(&builder);

  menuitem_property_set_variant(item, kPropertyShortcut, outside_array);
}

void GlobalMenuBarX11::OnBookmarkBarVisibilityChanged() {
  CommandIDMenuItemMap::iterator it =
      id_to_menu_item_.find(IDC_SHOW_BOOKMARK_BAR);
  if (it != id_to_menu_item_.end()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    // Note: Unlike the GTK version, we don't appear to need to do tricks where
    // we block activation while setting the toggle.
    menuitem_property_set_int(it->second, kPropertyToggleState,
                              prefs->GetBoolean(prefs::kShowBookmarkBar));
  }
}

void GlobalMenuBarX11::EnabledStateChangedForCommand(int id, bool enabled) {
  CommandIDMenuItemMap::iterator it = id_to_menu_item_.find(id);
  if (it != id_to_menu_item_.end())
    menuitem_property_set_bool(it->second, kPropertyEnabled, enabled);
}

void GlobalMenuBarX11::OnWindowMapped(unsigned long xid) {
  if (!server_)
    InitServer(xid);

  GlobalMenuBarRegistrarX11::GetInstance()->OnWindowMapped(xid);
}

void GlobalMenuBarX11::OnWindowUnmapped(unsigned long xid) {
  GlobalMenuBarRegistrarX11::GetInstance()->OnWindowUnmapped(xid);
}

void GlobalMenuBarX11::OnItemActivated(DbusmenuMenuitem* item,
                                       unsigned int timestamp) {
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "command-id"));
  chrome::ExecuteCommand(browser_, id);
}
