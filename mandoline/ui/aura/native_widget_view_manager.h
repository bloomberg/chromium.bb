// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_NATIVE_WIDGET_VIEW_MANAGER_H_
#define MANDOLINE_UI_AURA_NATIVE_WIDGET_VIEW_MANAGER_H_

#include "components/mus/public/cpp/view_observer.h"
#include "ui/views/widget/native_widget_aura.h"

namespace aura {
namespace client {
class DefaultCaptureClient;
}
}

namespace ui {
namespace internal {
class InputMethodDelegate;
}
}

namespace wm {
class FocusController;
}

namespace mojo {
class Shell;
}

namespace mandoline {

class WindowTreeHostMojo;

class NativeWidgetViewManager : public views::NativeWidgetAura,
                                public mus::ViewObserver {
 public:
  NativeWidgetViewManager(views::internal::NativeWidgetDelegate* delegate,
                          mojo::Shell* shell,
                          mus::View* view);
  ~NativeWidgetViewManager() override;

 private:
  // Overridden from internal::NativeWidgetAura:
  void InitNativeWidget(const views::Widget::InitParams& in_params) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // ViewObserver:
  void OnViewDestroyed(mus::View* view) override;
  void OnViewBoundsChanged(mus::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewFocusChanged(mus::View* gained_focus,
                          mus::View* lost_focus) override;
  void OnViewInputEvent(mus::View* view, const mojo::EventPtr& event) override;

  scoped_ptr<WindowTreeHostMojo> window_tree_host_;

  scoped_ptr<wm::FocusController> focus_client_;

  mus::View* view_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetViewManager);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_NATIVE_WIDGET_VIEW_MANAGER_H_
