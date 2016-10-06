// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_NON_CLIENT_FRAME_CONTROLLER_H_
#define ASH_MUS_NON_CLIENT_FRAME_CONTROLLER_H_

#include <stdint.h>

#include "ash/mus/frame/detached_title_area_renderer_host.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "services/ui/public/cpp/window_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace gfx {
class Insets;
}

namespace ui {
class Window;
class WindowManagerClient;
}

namespace ash {
namespace mus {

// Provides the non-client frame for mus Windows.
class NonClientFrameController : public views::WidgetDelegateView,
                                 public ui::WindowObserver,
                                 public DetachedTitleAreaRendererHost {
 public:
  // NonClientFrameController deletes itself when |window| is destroyed.
  static void Create(ui::Window* parent,
                     ui::Window* window,
                     ui::WindowManagerClient* window_manager_client);

  // Returns the preferred client area insets.
  static gfx::Insets GetPreferredClientAreaInsets();

  // Returns the width needed to display the standard set of buttons on the
  // title bar.
  static int GetMaxTitleBarButtonWidth();

  ui::Window* window() { return window_; }

 private:
  NonClientFrameController(ui::Window* parent,
                           ui::Window* window,
                           ui::WindowManagerClient* window_manager_client);
  ~NonClientFrameController() override;

  // DetachedTitleAreaRendererHost:
  void OnDetachedTitleAreaRendererDestroyed(
      DetachedTitleAreaRenderer* renderer) override;

  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool ShouldShowWindowTitle() const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // ui::WindowObserver:
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnWindowSharedPropertyChanged(
      ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowLocalPropertyChanged(ui::Window* window,
                                    const void* key,
                                    intptr_t old) override;
  void OnWindowDestroyed(ui::Window* window) override;

  views::Widget* widget_;

  // WARNING: as widget delays destruction there is a portion of time when this
  // is null.
  ui::Window* window_;

  // Used if a child window is added that has the
  // kRendererParentTitleArea_Property set.
  DetachedTitleAreaRenderer* detached_title_area_renderer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_NON_CLIENT_FRAME_CONTROLLER_H_
