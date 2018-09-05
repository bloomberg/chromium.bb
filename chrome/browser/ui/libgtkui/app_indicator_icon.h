// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_APP_INDICATOR_ICON_H_
#define CHROME_BROWSER_UI_LIBGTKUI_APP_INDICATOR_ICON_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/views/linux_ui/status_icon_linux.h"

typedef struct _AppIndicator AppIndicator;
typedef struct _GtkWidget GtkWidget;

class SkBitmap;

namespace gfx {
class ImageSkia;
}

namespace ui {
class MenuModel;
}

namespace libgtkui {
class AppIndicatorIconMenu;

// Status icon implementation which uses libappindicator.
class AppIndicatorIcon : public views::StatusIconLinux {
 public:
  // The id uniquely identifies the new status icon from other chrome status
  // icons.
  AppIndicatorIcon(std::string id,
                   const gfx::ImageSkia& image,
                   const base::string16& tool_tip);
  ~AppIndicatorIcon() override;

  // Indicates whether libappindicator so could be opened.
  static bool CouldOpen();

  // Overridden from views::StatusIconLinux:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* menu) override;
  void RefreshPlatformContextMenu() override;

 private:
  struct SetImageFromFileParams {
    // The temporary directory in which the icon(s) were written.
    base::FilePath parent_temp_dir;

    // The icon theme path to pass to libappindicator.
    std::string icon_theme_path;

    // The icon name to pass to libappindicator.
    std::string icon_name;
  };

  // Writes |bitmap| to a temporary directory on a worker thread. The temporary
  // directory is selected based on KDE's quirks.
  static SetImageFromFileParams WriteKDE4TempImageOnWorkerThread(
      const SkBitmap& bitmap,
      const base::FilePath& existing_temp_dir);

  // Writes |bitmap| to a temporary directory on a worker thread. The temporary
  // directory is selected based on Unity's quirks.
  static SetImageFromFileParams WriteUnityTempImageOnWorkerThread(
      const SkBitmap& bitmap,
      int icon_change_count,
      const std::string& id);

  void SetImageFromFile(const SetImageFromFileParams& params);
  void SetMenu();

  // Sets a menu item at the top of the menu as a replacement for the status
  // icon click action. Clicking on this menu item should simulate a status icon
  // click by despatching a click event.
  void UpdateClickActionReplacementMenuItem();

  // Callback for when the status icon click replacement menu item is activated.
  void OnClickActionReplacementMenuItemActivated();

  std::string id_;
  std::string tool_tip_;

  // Used to select KDE or Unity for image setting.
  base::nix::DesktopEnvironment desktop_env_;

  // Gtk status icon wrapper
  AppIndicator* icon_;

  std::unique_ptr<AppIndicatorIconMenu> menu_;
  ui::MenuModel* menu_model_;

  base::FilePath temp_dir_;
  int icon_change_count_;

  base::WeakPtrFactory<AppIndicatorIcon> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppIndicatorIcon);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_APP_INDICATOR_ICON_H_
