// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "ui/base/base_window.h"

class ArcAppWindowLauncherController;
class ArcAppWindowLauncherItemController;
namespace gfx {
class ImageSkia;
}

namespace views {
class Widget;
}

// A ui::BaseWindow for a chromeos launcher to control ARC applications.
class ArcAppWindow : public ui::BaseWindow, public ImageDecoder::ImageRequest {
 public:
  // TODO(khmel): use a bool set to false by default, or use an existing enum,
  // like ash::mojom::WindowStateType.
  enum class FullScreenMode {
    NOT_DEFINED,  // Fullscreen mode was not defined.
    ACTIVE,       // Fullscreen is activated for an app.
    NON_ACTIVE,   // Fullscreen was not activated for an app.
  };

  ArcAppWindow(int task_id,
               const arc::ArcAppShelfId& app_shelf_id,
               views::Widget* widget,
               ArcAppWindowLauncherController* owner);

  ~ArcAppWindow() override;

  void SetController(ArcAppWindowLauncherItemController* controller);

  void SetFullscreenMode(FullScreenMode mode);

  // Sets optional window title and icon. Note that |unsafe_icon_data_png| has
  // to be decoded in separate process for security reason.
  void SetDescription(const std::string& title,
                      const std::vector<uint8_t>& unsafe_icon_data_png);

  FullScreenMode fullscreen_mode() const { return fullscreen_mode_; }

  int task_id() const { return task_id_; }

  const arc::ArcAppShelfId& app_shelf_id() const { return app_shelf_id_; }

  const ash::ShelfID& shelf_id() const { return shelf_id_; }

  void set_shelf_id(const ash::ShelfID& shelf_id) { shelf_id_ = shelf_id; }

  views::Widget* widget() const { return widget_; }

  ArcAppWindowLauncherItemController* controller() { return controller_; }

  // ui::BaseWindow:
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() const override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  bool IsAlwaysOnTop() const override;
  void SetAlwaysOnTop(bool always_on_top) override;

 private:
  // Sets the icon for the window.
  void SetIcon(const gfx::ImageSkia& icon);

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;

  // Keeps associated ARC task id.
  const int task_id_;
  // Keeps ARC shelf grouping id.
  const arc::ArcAppShelfId app_shelf_id_;
  // Keeps shelf id.
  ash::ShelfID shelf_id_;
  // Keeps current full-screen mode.
  FullScreenMode fullscreen_mode_ = FullScreenMode::NOT_DEFINED;
  // Unowned pointers
  views::Widget* const widget_;
  ArcAppWindowLauncherController* const owner_;
  ArcAppWindowLauncherItemController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcAppWindow);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_APP_WINDOW_H_
