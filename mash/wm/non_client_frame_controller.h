// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_NON_CLIENT_FRAME_CONTROLLER_H_
#define MASH_WM_NON_CLIENT_FRAME_CONTROLLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/mus/public/cpp/window_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace gfx {
class Insets;
}

namespace mojo {
class Shell;
}

namespace mus {
class Window;
namespace mojom {
class WindowTreeHost;
}
}

namespace mash {
namespace wm {

// Provides the non-client frame for mus Windows.
class NonClientFrameController : public views::WidgetDelegateView,
                                 public mus::WindowObserver {
 public:
  // NonClientFrameController deletes itself when |window| is destroyed.
  NonClientFrameController(mojo::Shell* shell,
                           mus::Window* window,
                           mus::mojom::WindowTreeHost* window_tree_host);

  // Returns the preferred client area insets.
  static gfx::Insets GetPreferredClientAreaInsets();

  // Returns the width needed to display the standard set of buttons on the
  // title bar.
  static int GetMaxTitleBarButtonWidth();

  mus::Window* window() { return window_; }

 private:
  ~NonClientFrameController() override;

  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override;
  views::View* GetContentsView() override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // mus::WindowObserver:
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowDestroyed(mus::Window* window) override;

  views::Widget* widget_;

  // WARNING: as widget delays destruction there is a portion of time when this
  // is null.
  mus::Window* window_;

  mus::mojom::WindowTreeHost* mus_window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameController);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_NON_CLIENT_FRAME_CONTROLLER_H_
