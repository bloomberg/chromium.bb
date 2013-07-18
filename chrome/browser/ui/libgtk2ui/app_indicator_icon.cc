// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/app_indicator_icon.h"

#include <gtk/gtk.h>
#include <dlfcn.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/ui/libgtk2ui/menu_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image_skia.h"

namespace {

typedef enum {
  APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
  APP_INDICATOR_CATEGORY_COMMUNICATIONS,
  APP_INDICATOR_CATEGORY_SYSTEM_SERVICES,
  APP_INDICATOR_CATEGORY_HARDWARE,
  APP_INDICATOR_CATEGORY_OTHER
} AppIndicatorCategory;

typedef enum {
  APP_INDICATOR_STATUS_PASSIVE,
  APP_INDICATOR_STATUS_ACTIVE,
  APP_INDICATOR_STATUS_ATTENTION
} AppIndicatorStatus;

typedef AppIndicator* (*app_indicator_new_func)(const gchar* id,
                                                const gchar* icon_name,
                                                AppIndicatorCategory category);

typedef AppIndicator* (*app_indicator_new_with_path_func)(
    const gchar* id,
    const gchar* icon_name,
    AppIndicatorCategory category,
    const gchar* icon_theme_path);

typedef void (*app_indicator_set_status_func)(AppIndicator* self,
                                              AppIndicatorStatus status);

typedef void (*app_indicator_set_attention_icon_full_func)(
    AppIndicator* self,
    const gchar* icon_name,
    const gchar* icon_desc);

typedef void (*app_indicator_set_menu_func)(AppIndicator* self, GtkMenu* menu);

typedef void (*app_indicator_set_icon_full_func)(AppIndicator* self,
                                                 const gchar* icon_name,
                                                 const gchar* icon_desc);

typedef void (*app_indicator_set_icon_theme_path_func)(
    AppIndicator* self,
    const gchar* icon_theme_path);

bool attempted_load = false;
bool opened = false;

// Retrieved functions from libappindicator.
app_indicator_new_func app_indicator_new = NULL;
app_indicator_new_with_path_func app_indicator_new_with_path = NULL;
app_indicator_set_status_func app_indicator_set_status = NULL;
app_indicator_set_attention_icon_full_func
    app_indicator_set_attention_icon_full = NULL;
app_indicator_set_menu_func app_indicator_set_menu = NULL;
app_indicator_set_icon_full_func app_indicator_set_icon_full = NULL;
app_indicator_set_icon_theme_path_func app_indicator_set_icon_theme_path = NULL;

void EnsureMethodsLoaded() {

  if (attempted_load)
    return;
  attempted_load = true;

  void* indicator_lib = dlopen("libappindicator.so", RTLD_LAZY);
  if (!indicator_lib) {
    indicator_lib = dlopen("libappindicator.so.1", RTLD_LAZY);
  }
  if (!indicator_lib) {
    indicator_lib = dlopen("libappindicator.so.0", RTLD_LAZY);
  }
  if (!indicator_lib) {
    return;
  }

  opened = true;

  app_indicator_new = reinterpret_cast<app_indicator_new_func>(
      dlsym(indicator_lib, "app_indicator_new"));

  app_indicator_new_with_path =
      reinterpret_cast<app_indicator_new_with_path_func>(
          dlsym(indicator_lib, "app_indicator_new_with_path"));

  app_indicator_set_status = reinterpret_cast<app_indicator_set_status_func>(
      dlsym(indicator_lib, "app_indicator_set_status"));

  app_indicator_set_attention_icon_full =
      reinterpret_cast<app_indicator_set_attention_icon_full_func>(
          dlsym(indicator_lib, "app_indicator_set_attention_icon_full"));

  app_indicator_set_menu = reinterpret_cast<app_indicator_set_menu_func>(
      dlsym(indicator_lib, "app_indicator_set_menu"));

  app_indicator_set_icon_full =
      reinterpret_cast<app_indicator_set_icon_full_func>(
          dlsym(indicator_lib, "app_indicator_set_icon_full"));

  app_indicator_set_icon_theme_path =
      reinterpret_cast<app_indicator_set_icon_theme_path_func>(
          dlsym(indicator_lib, "app_indicator_set_icon_theme_path"));
}

}  // namespace

namespace libgtk2ui {

AppIndicatorIcon::AppIndicatorIcon(std::string id)
    : id_(id),
      icon_(NULL),
      gtk_menu_(NULL),
      menu_model_(NULL),
      icon_change_count_(0),
      block_activation_(false),
      has_click_action_replacement_(false) {
  EnsureMethodsLoaded();
}
AppIndicatorIcon::~AppIndicatorIcon() {
  if (icon_) {
    app_indicator_set_status(icon_, APP_INDICATOR_STATUS_PASSIVE);
    if (menu_model_)
      menu_model_->MenuClosed();
    if (gtk_menu_)
      DestroyMenu();
    g_object_unref(icon_);
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&AppIndicatorIcon::DeletePath, icon_file_path_.DirName()));
  }
}

bool AppIndicatorIcon::CouldOpen() {
  EnsureMethodsLoaded();
  return opened;
}

void AppIndicatorIcon::SetImage(const gfx::ImageSkia& image) {
  if (opened) {
    ++icon_change_count_;
    gfx::ImageSkia safe_image = gfx::ImageSkia(image);
    safe_image.MakeThreadSafe();
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool()
            ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN).get(),
        FROM_HERE,
        base::Bind(&AppIndicatorIcon::CreateTempImageFile,
                   safe_image,
                   icon_change_count_,
                   id_),
        base::Bind(&AppIndicatorIcon::SetImageFromFile,
                   base::Unretained(this)));
  }
}

void AppIndicatorIcon::SetPressedImage(const gfx::ImageSkia& image) {
  // Ignore pressed images, since the standard on Linux is to not highlight
  // pressed status icons.
}

void AppIndicatorIcon::SetToolTip(const string16& tool_tip) {
  // App-indicators don't support tool-tips. Ignore call.
}

void AppIndicatorIcon::SetClickActionLabel(const string16& label) {
  click_action_label_ = UTF16ToUTF8(label);

  // If the menu item has already been created, then find the menu item and
  // change it's label.
  if (has_click_action_replacement_) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(gtk_menu_));
    for (GList* child = children; child; child = g_list_next(child))
      if (g_object_get_data(G_OBJECT(child->data), "click-action-item") !=
          NULL) {
        gtk_menu_item_set_label(GTK_MENU_ITEM(child->data),
                                click_action_label_.c_str());
        break;
      }
    g_list_free(children);
  } else if (icon_) {
    CreateClickActionReplacement();
    app_indicator_set_menu(icon_, GTK_MENU(gtk_menu_));
  }
}

void AppIndicatorIcon::UpdatePlatformContextMenu(ui::MenuModel* model) {
  if (!opened)
    return;

  if (gtk_menu_) {
    DestroyMenu();
    has_click_action_replacement_ = false;
  }
  menu_model_ = model;

  // If icon doesn't exist now it's okay, the menu will be set later along with
  // the image. Both an icon and a menu are required to show an app indicator.
  if (model && icon_)
    SetMenu();
}

void AppIndicatorIcon::SetImageFromFile(base::FilePath icon_file_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!icon_file_path.empty()) {
    base::FilePath old_path = icon_file_path_;
    icon_file_path_ = icon_file_path;

    std::string icon_name =
        icon_file_path_.BaseName().RemoveExtension().value();
    std::string icon_dir = icon_file_path_.DirName().value();
    if (!icon_) {
      icon_ =
          app_indicator_new_with_path(id_.c_str(),
                                      icon_name.c_str(),
                                      APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
                                      icon_dir.c_str());
      app_indicator_set_status(icon_, APP_INDICATOR_STATUS_ACTIVE);
      if (menu_model_) {
        SetMenu();
      } else if (!click_action_label_.empty()) {
        CreateClickActionReplacement();
        app_indicator_set_menu(icon_, GTK_MENU(gtk_menu_));
      }
    } else {
      // Currently we are creating a new temp directory every time the icon is
      // set. So we need to set the directory each time.
      app_indicator_set_icon_theme_path(icon_, icon_dir.c_str());
      app_indicator_set_icon_full(icon_, icon_name.c_str(), "icon");

      // Delete previous icon directory.
      content::BrowserThread::GetBlockingPool()->PostTask(
          FROM_HERE,
          base::Bind(&AppIndicatorIcon::DeletePath, old_path.DirName()));
    }
  }
}

void AppIndicatorIcon::SetMenu() {
  gtk_menu_ = gtk_menu_new();
  BuildSubmenuFromModel(menu_model_,
                        gtk_menu_,
                        G_CALLBACK(OnMenuItemActivatedThunk),
                        &block_activation_,
                        this);
  if (!click_action_label_.empty())
    CreateClickActionReplacement();
  UpdateMenu();
  menu_model_->MenuWillShow();
  app_indicator_set_menu(icon_, GTK_MENU(gtk_menu_));
}

void AppIndicatorIcon::CreateClickActionReplacement() {
  GtkWidget* menu_item = NULL;

  // If a menu doesn't exist create one just for the click action replacement.
  if (!gtk_menu_) {
    gtk_menu_ = gtk_menu_new();
  } else {
    // Add separator before the other menu items.
    menu_item = gtk_separator_menu_item_new();
    gtk_widget_show(menu_item);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(gtk_menu_), menu_item);
  }

  // Add "click replacement menu item".
  menu_item = gtk_menu_item_new_with_mnemonic(click_action_label_.c_str());
  g_object_set_data(
      G_OBJECT(menu_item), "click-action-item", GINT_TO_POINTER(1));
  g_signal_connect(menu_item, "activate", G_CALLBACK(OnClickThunk), this);
  gtk_widget_show(menu_item);
  gtk_menu_shell_prepend(GTK_MENU_SHELL(gtk_menu_), menu_item);

  has_click_action_replacement_ = true;
}

void AppIndicatorIcon::DestroyMenu() {
  if (menu_model_)
    menu_model_->MenuClosed();
  gtk_widget_destroy(gtk_menu_);
  gtk_menu_ = NULL;
  menu_model_ = NULL;
}

base::FilePath AppIndicatorIcon::CreateTempImageFile(gfx::ImageSkia image,
                                                     int icon_change_count,
                                                     std::string id) {
  scoped_refptr<base::RefCountedMemory> png_data =
      gfx::Image(image).As1xPNGBytes();
  if (png_data->size() == 0) {
    // If the bitmap could not be encoded to PNG format, skip it.
    LOG(WARNING) << "Could not encode icon";
    return base::FilePath();
  }

  base::FilePath temp_dir;
  base::FilePath new_file_path;

  // Create a new temporary directory for each image since using a single
  // temporary directory seems to have issues when changing icons in quick
  // succession.
  if (!file_util::CreateNewTempDirectory("", &temp_dir))
    return base::FilePath();
  new_file_path =
      temp_dir.Append(id + base::StringPrintf("_%d.png", icon_change_count));
  int bytes_written =
      file_util::WriteFile(new_file_path,
                           reinterpret_cast<const char*>(png_data->front()),
                           png_data->size());

  if (bytes_written != static_cast<int>(png_data->size())) {
    return base::FilePath();
  }

  return new_file_path;
}

void AppIndicatorIcon::DeletePath(base::FilePath icon_file_path) {
  if (!icon_file_path.empty()) {
    base::DeleteFile(icon_file_path, true);
  }
}

void AppIndicatorIcon::UpdateMenu() {
  gtk_container_foreach(
      GTK_CONTAINER(gtk_menu_), SetMenuItemInfo, &block_activation_);
}

void AppIndicatorIcon::OnClick(GtkWidget* menu_item) {
  if (delegate())
    delegate()->OnClick();
}

void AppIndicatorIcon::OnMenuItemActivated(GtkWidget* menu_item) {
  if (block_activation_)
    return;

  ui::MenuModel* model = ModelForMenuItem(GTK_MENU_ITEM(menu_item));

  if (!model) {
    // There won't be a model for "native" submenus like the "Input Methods"
    // context menu. We don't need to handle activation messages for submenus
    // anyway, so we can just return here.
    DCHECK(gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item)));
    return;
  }

  // The activate signal is sent to radio items as they get deselected;
  // ignore it in this case.
  if (GTK_IS_RADIO_MENU_ITEM(menu_item) &&
      !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item))) {
    return;
  }

  int id;
  if (!GetMenuItemID(menu_item, &id))
    return;

  // The menu item can still be activated by hotkeys even if it is disabled.
  if (menu_model_->IsEnabledAt(id))
    ExecuteCommand(model, id);
  UpdateMenu();
}

}  // namespace libgtk2ui
