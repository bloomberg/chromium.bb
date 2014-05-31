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
#include "chrome/browser/ui/libgtk2ui/app_indicator_icon_menu.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image.h"
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

bool g_attempted_load = false;
bool g_opened = false;

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
  if (g_attempted_load)
    return;

  g_attempted_load = true;

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

  g_opened = true;

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

base::FilePath CreateTempImageFile(gfx::ImageSkia* image_ptr,
                                   int icon_change_count,
                                   std::string id) {
  scoped_ptr<gfx::ImageSkia> image(image_ptr);

  scoped_refptr<base::RefCountedMemory> png_data =
      gfx::Image(*image.get()).As1xPNGBytes();
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
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir))
    return base::FilePath();
  new_file_path =
      temp_dir.Append(id + base::StringPrintf("_%d.png", icon_change_count));
  int bytes_written =
      base::WriteFile(new_file_path,
                      png_data->front_as<char>(), png_data->size());

  if (bytes_written != static_cast<int>(png_data->size()))
    return base::FilePath();
  return new_file_path;
}

void DeleteTempImagePath(const base::FilePath& icon_file_path) {
  if (icon_file_path.empty())
    return;
  base::DeleteFile(icon_file_path, true);
}

}  // namespace

namespace libgtk2ui {

AppIndicatorIcon::AppIndicatorIcon(std::string id,
                                   const gfx::ImageSkia& image,
                                   const base::string16& tool_tip)
    : id_(id),
      icon_(NULL),
      menu_model_(NULL),
      icon_change_count_(0),
      weak_factory_(this) {
  EnsureMethodsLoaded();
  tool_tip_ = base::UTF16ToUTF8(tool_tip);
  SetImage(image);
}
AppIndicatorIcon::~AppIndicatorIcon() {
  if (icon_) {
    app_indicator_set_status(icon_, APP_INDICATOR_STATUS_PASSIVE);
    g_object_unref(icon_);
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&DeleteTempImagePath, icon_file_path_.DirName()));
  }
}

// static
bool AppIndicatorIcon::CouldOpen() {
  EnsureMethodsLoaded();
  return g_opened;
}

void AppIndicatorIcon::SetImage(const gfx::ImageSkia& image) {
  if (!g_opened)
    return;

  ++icon_change_count_;

  // We create a deep copy of the image since it may have been freed by the time
  // it's accessed in the other thread.
  scoped_ptr<gfx::ImageSkia> safe_image(image.DeepCopy());
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN).get(),
      FROM_HERE,
      base::Bind(&CreateTempImageFile,
                 safe_image.release(),
                 icon_change_count_,
                 id_),
      base::Bind(&AppIndicatorIcon::SetImageFromFile,
                 weak_factory_.GetWeakPtr()));
}

void AppIndicatorIcon::SetPressedImage(const gfx::ImageSkia& image) {
  // Ignore pressed images, since the standard on Linux is to not highlight
  // pressed status icons.
}

void AppIndicatorIcon::SetToolTip(const base::string16& tool_tip) {
  DCHECK(!tool_tip_.empty());
  tool_tip_ = base::UTF16ToUTF8(tool_tip);
  UpdateClickActionReplacementMenuItem();
}

void AppIndicatorIcon::UpdatePlatformContextMenu(ui::MenuModel* model) {
  if (!g_opened)
    return;

  menu_model_ = model;

  // The icon is created asynchronously so it might not exist when the menu is
  // set.
  if (icon_)
    SetMenu();
}

void AppIndicatorIcon::RefreshPlatformContextMenu() {
  menu_->Refresh();
}

void AppIndicatorIcon::SetImageFromFile(const base::FilePath& icon_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (icon_file_path.empty())
    return;

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
    SetMenu();
  } else {
    // Currently we are creating a new temp directory every time the icon is
    // set. So we need to set the directory each time.
    app_indicator_set_icon_theme_path(icon_, icon_dir.c_str());
    app_indicator_set_icon_full(icon_, icon_name.c_str(), "icon");

    // Delete previous icon directory.
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&DeleteTempImagePath, old_path.DirName()));
  }
}

void AppIndicatorIcon::SetMenu() {
  menu_.reset(new AppIndicatorIconMenu(menu_model_));
  UpdateClickActionReplacementMenuItem();
  app_indicator_set_menu(icon_, menu_->GetGtkMenu());
}

void AppIndicatorIcon::UpdateClickActionReplacementMenuItem() {
  // The menu may not have been created yet.
  if (!menu_.get())
    return;

  if (!delegate()->HasClickAction() && menu_model_)
    return;

  DCHECK(!tool_tip_.empty());
  menu_->UpdateClickActionReplacementMenuItem(
      tool_tip_.c_str(),
      base::Bind(&AppIndicatorIcon::OnClickActionReplacementMenuItemActivated,
                 base::Unretained(this)));
}

void AppIndicatorIcon::OnClickActionReplacementMenuItemActivated() {
  if (delegate())
    delegate()->OnClick();
}

}  // namespace libgtk2ui
