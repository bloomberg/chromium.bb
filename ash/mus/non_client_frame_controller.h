// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_NON_CLIENT_FRAME_CONTROLLER_H_
#define ASH_MUS_NON_CLIENT_FRAME_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
class WindowManagerClient;
}

namespace gfx {
class Insets;
}

namespace ui {
namespace mojom {
enum class WindowType;
}
}

namespace ash {
namespace mus {

class WindowManager;

// Provides the non-client frame for mus Windows.
class NonClientFrameController
    : public views::WidgetDelegateView,
      public aura::WindowObserver,
      public aura::client::TransientWindowClientObserver {
 public:
  // Creates a new NonClientFrameController and window to render the non-client
  // frame decorations. This deletes itself when |window| is destroyed. |parent|
  // is the parent to place the newly created window in, and may be null. If
  // |parent| is null |context| is used to determine the parent Window. One of
  // |parent| or |context| must be non-null.
  NonClientFrameController(
      aura::Window* parent,
      aura::Window* context,
      const gfx::Rect& bounds,
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties,
      WindowManager* window_manager);

  // Returns the NonClientFrameController for the specified window, null if
  // one was not created.
  static NonClientFrameController* Get(aura::Window* window);

  // Returns the preferred client area insets.
  static gfx::Insets GetPreferredClientAreaInsets();

  // Returns the width needed to display the standard set of buttons on the
  // title bar.
  static int GetMaxTitleBarButtonWidth();

  aura::Window* window() { return window_; }

  aura::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  void SetClientArea(const gfx::Insets& insets,
                     const std::vector<gfx::Rect>& additional_client_areas);

 private:
  ~NonClientFrameController() override;

  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool ShouldShowWindowTitle() const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroyed(aura::Window* window) override;

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

  aura::WindowManagerClient* window_manager_client_;

  views::Widget* widget_;

  // WARNING: as widget delays destruction there is a portion of time when this
  // is null.
  aura::Window* window_;

  bool did_init_native_widget_ = false;

  gfx::Insets client_area_insets_;
  std::vector<gfx::Rect> additional_client_areas_;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_NON_CLIENT_FRAME_CONTROLLER_H_
