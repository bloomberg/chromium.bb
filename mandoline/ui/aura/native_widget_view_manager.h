// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_NATIVE_WIDGET_VIEW_MANAGER_H_
#define MANDOLINE_UI_AURA_NATIVE_WIDGET_VIEW_MANAGER_H_

#include "components/view_manager/public/cpp/view_observer.h"
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
                                       public mojo::ViewObserver {
 public:
  NativeWidgetViewManager(views::internal::NativeWidgetDelegate* delegate,
                          mojo::Shell* shell,
                          mojo::View* view);
  ~NativeWidgetViewManager() override;

 private:
  // Overridden from internal::NativeWidgetAura:
  void InitNativeWidget(const views::Widget::InitParams& in_params) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // ViewObserver:
  void OnViewDestroyed(mojo::View* view) override;
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewInputEvent(mojo::View* view, const mojo::EventPtr& event) override;

  scoped_ptr<WindowTreeHostMojo> window_tree_host_;

  scoped_ptr<wm::FocusController> focus_client_;

  mojo::View* view_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetViewManager);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_NATIVE_WIDGET_VIEW_MANAGER_H_
