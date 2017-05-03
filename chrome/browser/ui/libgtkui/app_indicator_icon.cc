// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/app_indicator_icon.h"

#include <dlfcn.h>
#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/md5.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/ui/libgtkui/app_indicator_icon_menu.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/codec/png_codec.h"
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
app_indicator_new_func app_indicator_new = nullptr;
app_indicator_new_with_path_func app_indicator_new_with_path = nullptr;
app_indicator_set_status_func app_indicator_set_status = nullptr;
app_indicator_set_attention_icon_full_func
    app_indicator_set_attention_icon_full = nullptr;
app_indicator_set_menu_func app_indicator_set_menu = nullptr;
app_indicator_set_icon_full_func app_indicator_set_icon_full = nullptr;
app_indicator_set_icon_theme_path_func app_indicator_set_icon_theme_path =
    nullptr;

void EnsureMethodsLoaded() {
  if (g_attempted_load)
    return;

  g_attempted_load = true;

  // Only use libappindicator where it is needed to support dbus based status
  // icons. In particular, libappindicator does not support a click action.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment environment =
      base::nix::GetDesktopEnvironment(env.get());
  if (environment != base::nix::DESKTOP_ENVIRONMENT_KDE4 &&
      environment != base::nix::DESKTOP_ENVIRONMENT_KDE5 &&
      environment != base::nix::DESKTOP_ENVIRONMENT_UNITY) {
    return;
  }

  void* indicator_lib = nullptr;

  // These include guards might be unnecessary, but let's keep them as a
  // precaution since using gtk2 and gtk3 symbols in the same process is
  // explicitly unsupported.
#if GTK_MAJOR_VERSION == 2
  if (!indicator_lib)
    indicator_lib = dlopen("libappindicator.so", RTLD_LAZY);

  if (!indicator_lib)
    indicator_lib = dlopen("libappindicator.so.1", RTLD_LAZY);

  if (!indicator_lib)
    indicator_lib = dlopen("libappindicator.so.0", RTLD_LAZY);
#endif

#if GTK_MAJOR_VERSION == 3
  if (!indicator_lib)
    indicator_lib = dlopen("libappindicator3.so", RTLD_LAZY);

  if (!indicator_lib)
    indicator_lib = dlopen("libappindicator3.so.1", RTLD_LAZY);
#endif

  if (!indicator_lib)
    return;

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

// Writes |bitmap| to a file at |path|. Returns true if successful.
bool WriteFile(const base::FilePath& path, const SkBitmap& bitmap) {
  std::vector<unsigned char> png_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_data))
    return false;
  int bytes_written = base::WriteFile(
      path, reinterpret_cast<char*>(&png_data[0]), png_data.size());
  return (bytes_written == static_cast<int>(png_data.size()));
}

void DeleteTempDirectory(const base::FilePath& dir_path) {
  if (dir_path.empty())
    return;
  base::DeleteFile(dir_path, true);
}

}  // namespace

namespace libgtkui {

AppIndicatorIcon::AppIndicatorIcon(std::string id,
                                   const gfx::ImageSkia& image,
                                   const base::string16& tool_tip)
    : id_(id),
      icon_(nullptr),
      menu_model_(nullptr),
      icon_change_count_(0),
      weak_factory_(this) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  desktop_env_ = base::nix::GetDesktopEnvironment(env.get());

  EnsureMethodsLoaded();
  tool_tip_ = base::UTF16ToUTF8(tool_tip);
  SetImage(image);
}
AppIndicatorIcon::~AppIndicatorIcon() {
  if (icon_) {
    app_indicator_set_status(icon_, APP_INDICATOR_STATUS_PASSIVE);
    g_object_unref(icon_);
    base::PostTaskWithTraits(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BACKGROUND},
                             base::BindOnce(&DeleteTempDirectory, temp_dir_));
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

  // Copy the bitmap because it may be freed by the time it's accessed in
  // another thread.
  SkBitmap safe_bitmap = *image.bitmap();

  const base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

  if (desktop_env_ == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
      desktop_env_ == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, kTraits,
        base::Bind(AppIndicatorIcon::WriteKDE4TempImageOnWorkerThread,
                   safe_bitmap, temp_dir_),
        base::Bind(&AppIndicatorIcon::SetImageFromFile,
                   weak_factory_.GetWeakPtr()));
  } else {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, kTraits,
        base::Bind(AppIndicatorIcon::WriteUnityTempImageOnWorkerThread,
                   safe_bitmap, icon_change_count_, id_),
        base::Bind(&AppIndicatorIcon::SetImageFromFile,
                   weak_factory_.GetWeakPtr()));
  }
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

// static
AppIndicatorIcon::SetImageFromFileParams
AppIndicatorIcon::WriteKDE4TempImageOnWorkerThread(
    const SkBitmap& bitmap,
    const base::FilePath& existing_temp_dir) {
  base::FilePath temp_dir = existing_temp_dir;
  if (temp_dir.empty() &&
      !base::CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir)) {
    LOG(WARNING) << "Could not create temporary directory";
    return SetImageFromFileParams();
  }

  base::FilePath icon_theme_path = temp_dir.AppendASCII("icons");

  // On KDE4, an image located in a directory ending with
  // "icons/hicolor/24x24/apps" can be used as the app indicator image because
  // "/usr/share/icons/hicolor/24x24/apps" exists.
  base::FilePath image_dir = icon_theme_path.AppendASCII("hicolor")
                                 .AppendASCII("24x24")
                                 .AppendASCII("apps");

  if (!base::CreateDirectory(image_dir))
    return SetImageFromFileParams();

  // On KDE4, the name of the image file for each different looking bitmap must
  // be unique. It must also be unique across runs of Chrome.
  std::vector<unsigned char> bitmap_png_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_png_data)) {
    LOG(WARNING) << "Could not encode icon";
    return SetImageFromFileParams();
  }
  base::MD5Digest digest;
  base::MD5Sum(reinterpret_cast<char*>(&bitmap_png_data[0]),
               bitmap_png_data.size(), &digest);
  std::string icon_name = base::StringPrintf(
      "chrome_app_indicator2_%s", base::MD5DigestToBase16(digest).c_str());

  // If |bitmap| is not 24x24, KDE does some really ugly resizing. Pad |bitmap|
  // with transparent pixels to make it 24x24.
  const int kDesiredSize = 24;
  SkBitmap scaled_bitmap;
  scaled_bitmap.allocN32Pixels(kDesiredSize, kDesiredSize);
  scaled_bitmap.eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(scaled_bitmap);
  canvas.drawBitmap(bitmap, (kDesiredSize - bitmap.width()) / 2,
                    (kDesiredSize - bitmap.height()) / 2);

  base::FilePath image_path = image_dir.Append(icon_name + ".png");
  if (!WriteFile(image_path, scaled_bitmap))
    return SetImageFromFileParams();

  SetImageFromFileParams params;
  params.parent_temp_dir = temp_dir;
  params.icon_theme_path = icon_theme_path.value();
  params.icon_name = icon_name;
  return params;
}

// static
AppIndicatorIcon::SetImageFromFileParams
AppIndicatorIcon::WriteUnityTempImageOnWorkerThread(const SkBitmap& bitmap,
                                                    int icon_change_count,
                                                    const std::string& id) {
  // Create a new temporary directory for each image on Unity since using a
  // single temporary directory seems to have issues when changing icons in
  // quick succession.
  base::FilePath temp_dir;
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir)) {
    LOG(WARNING) << "Could not create temporary directory";
    return SetImageFromFileParams();
  }

  std::string icon_name =
      base::StringPrintf("%s_%d", id.c_str(), icon_change_count);
  base::FilePath image_path = temp_dir.Append(icon_name + ".png");
  SetImageFromFileParams params;
  if (WriteFile(image_path, bitmap)) {
    params.parent_temp_dir = temp_dir;
    params.icon_theme_path = temp_dir.value();
    params.icon_name = icon_name;
  }
  return params;
}

void AppIndicatorIcon::SetImageFromFile(const SetImageFromFileParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (params.icon_theme_path.empty())
    return;

  if (!icon_) {
    icon_ =
        app_indicator_new_with_path(id_.c_str(),
                                    params.icon_name.c_str(),
                                    APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
                                    params.icon_theme_path.c_str());
    app_indicator_set_status(icon_, APP_INDICATOR_STATUS_ACTIVE);
    SetMenu();
  } else {
    app_indicator_set_icon_theme_path(icon_, params.icon_theme_path.c_str());
    app_indicator_set_icon_full(icon_, params.icon_name.c_str(), "icon");
  }

  if (temp_dir_ != params.parent_temp_dir) {
    base::PostTaskWithTraits(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BACKGROUND},
                             base::BindOnce(&DeleteTempDirectory, temp_dir_));
    temp_dir_ = params.parent_temp_dir;
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

}  // namespace libgtkui
